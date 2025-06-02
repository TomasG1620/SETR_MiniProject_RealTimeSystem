/**
 * @file main.c
 * @brief Thermal Process Controller - Monolithic (botões, LEDs, I²C sensor, controlador ON/OFF)
 * @details
 *   - Botões: liga/desliga sistema, inc/dec setpoint via RTDB
 *   - LEDs: indicam estado ON/OFF, “normal”, “baixo” ou “alto” comparando current_temp x setpoint
 *   - Sensor TC74A0 via I²C: escreve RTR (0x00) e lê um byte (temperatura em °C), empurra ao RTDB
 *   - Controlador ON/OFF: liga/desliga MOSFET (p1.12) conforme histerese +-1°C sobre setpoint
 *   - UART: permite consultar current_temp e mudar max_temp via comandos “#C…!” e “#M…!”
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

#include "rtdb.h"
#include "uartcomm.h"
#include "controller.h"

#define BTN_NODE_ONOFF   DT_ALIAS(sw0)
#define BTN_NODE_INC     DT_ALIAS(sw1)
#define BTN_NODE_MENU    DT_ALIAS(sw2)  
#define BTN_NODE_DEC     DT_ALIAS(sw3)

#define BTN_ONOFF_DEV    DT_GPIO_CTLR(BTN_NODE_ONOFF, gpios)
#define BTN_ONOFF_PIN    DT_GPIO_PIN(BTN_NODE_ONOFF, gpios)
#define BTN_ONOFF_FLAGS  (DT_GPIO_FLAGS(BTN_NODE_ONOFF, gpios) | GPIO_INPUT)

#define BTN_INC_DEV      DT_GPIO_CTLR(BTN_NODE_INC, gpios)
#define BTN_INC_PIN      DT_GPIO_PIN(BTN_NODE_INC, gpios)
#define BTN_INC_FLAGS    (DT_GPIO_FLAGS(BTN_NODE_INC, gpios) | GPIO_INPUT)

#define BTN_MENU_DEV     DT_GPIO_CTLR(BTN_NODE_MENU, gpios)
#define BTN_MENU_PIN     DT_GPIO_PIN(BTN_NODE_MENU, gpios)
#define BTN_MENU_FLAGS   (DT_GPIO_FLAGS(BTN_NODE_MENU, gpios) | GPIO_INPUT)

#define BTN_DEC_DEV      DT_GPIO_CTLR(BTN_NODE_DEC, gpios)
#define BTN_DEC_PIN      DT_GPIO_PIN(BTN_NODE_DEC, gpios)
#define BTN_DEC_FLAGS    (DT_GPIO_FLAGS(BTN_NODE_DEC, gpios) | GPIO_INPUT)

/* --------------------------------------------------------------------------
 * Callbacks de GPIO e timestamps para debounce
 * -------------------------------------------------------------------------- */
static struct gpio_callback cb_onoff;
static struct gpio_callback cb_inc;
static struct gpio_callback cb_menu;
static struct gpio_callback cb_dec;

/* --------------------------------------------------------------------------
 * Função que imprime o menu de uso (quando SW2 for pressionado)
 * -------------------------------------------------------------------------- */
static void print_menu(void)
{
    printk("\n"
           "============================================\n"
           "      CONTROLE TÉRMICO – MENU DE USO\n"
           "============================================\n"
           " Botões Físicos (painel do nRF52840DK):\n"
           "   • SW0 (P0.11): alterna sistema ligado/desligado\n"
           "   • SW1 (P0.12): incrementa setpoint (+1 °C)\n"
           "   • SW2 (P0.24): exibe este menu de ajuda\n"
           "   • SW3 (P0.25): decrementa setpoint (-1 °C)\n"
           "\n"
           " LEDs (indicadores de estado):\n"
           "   • LED0 (P0.13): indica se o sistema está ligado (LED aceso = ON)\n"
           "   • LED1 (P0.14): TEMPERATURA NORMAL (|cur – sp| ≤ 2 °C)\n"
           "   • LED2 (P0.15): TEMPERATURA ABAIXO (cur < sp – 2 °C)\n"
           "   • LED3 (P0.16): TEMPERATURA ACIMA (cur > sp + 2 °C)\n"
           "\n"
           " Use estes botões para controlar ON/OFF e ajustar setpoint.\n"
           "============================================\n");
}

/* --------------------------------------------------------------------------
 * Callback SW0: ON/OFF 
 * -------------------------------------------------------------------------- */
static void onoff_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    bool on = !rtdb_get_system_on();
    rtdb_set_system_on(on);
    printk("\n[Botão SW0] Sistema agora: %s\n", on ? "ON" : "OFF");
}

/* --------------------------------------------------------------------------
 * Callback SW1: incrementa setpoint com debounce + checa max_temp
 * -------------------------------------------------------------------------- */
static void inc_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

    int16_t tentativa = rtdb_get_setpoint() + 1;
    rtdb_set_setpoint(tentativa);

    int16_t real = rtdb_get_setpoint();
    int16_t mx   = rtdb_get_max_temp();

    if (real >= mx && tentativa > mx) {
        printk("[Button] Temperatura máxima atingida (%d °C)\n", mx);
    } else {
        printk("[Button] Setpoint incrementado para %d °C\n", real);
    }
}

/* --------------------------------------------------------------------------
 * Callback SW2: exibe menu de uso com debounce
 * -------------------------------------------------------------------------- */
static void menu_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(cb);
    ARG_UNUSED(pins);

    print_menu();
}

/* --------------------------------------------------------------------------
 * Callback SW3: decrementa setpoint com debounce + checa min_temp
 * -------------------------------------------------------------------------- */
static void dec_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);

    int16_t tentativa = rtdb_get_setpoint() - 1;
    rtdb_set_setpoint(tentativa);

    int16_t real = rtdb_get_setpoint();
    int16_t mi   = rtdb_get_min_temp();

    if (real <= mi && tentativa < mi) {
        printk("[Button] Temperatura mínima atingida (%d °C)\n", mi);
    } else {
        printk("[Button] Setpoint decrementado para %d °C\n", real);
    }
}

/* --------------------------------------------------------------------------
 * Inicialização de todos os botões (com SW2 incluso)
 * -------------------------------------------------------------------------- */
void button_ctrl_init(void)
{
    const struct device *dev;

    /* --- SW0 (ON/OFF) --- */
    dev = DEVICE_DT_GET(BTN_ONOFF_DEV);
    __ASSERT(dev != NULL, "GPIO device for SW0 not found");
    gpio_pin_configure(dev, BTN_ONOFF_PIN, BTN_ONOFF_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_ONOFF_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_onoff, onoff_pressed, BIT(BTN_ONOFF_PIN));
    gpio_add_callback(dev, &cb_onoff);

    /* --- SW1 (incrementa setpoint) --- */
    dev = DEVICE_DT_GET(BTN_INC_DEV);
    __ASSERT(dev != NULL, "GPIO device for SW1 not found");
    gpio_pin_configure(dev, BTN_INC_PIN, BTN_INC_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_INC_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_inc, inc_pressed, BIT(BTN_INC_PIN));
    gpio_add_callback(dev, &cb_inc);

    /* --- SW2 (imprime menu) --- */
    dev = DEVICE_DT_GET(BTN_MENU_DEV);
    __ASSERT(dev != NULL, "GPIO device for SW2 not found");
    gpio_pin_configure(dev, BTN_MENU_PIN, BTN_MENU_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_MENU_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_menu, menu_pressed, BIT(BTN_MENU_PIN));
    gpio_add_callback(dev, &cb_menu);

    /* --- SW3 (decrementa setpoint) --- */
    dev = DEVICE_DT_GET(BTN_DEC_DEV);
    __ASSERT(dev != NULL, "GPIO device for SW3 not found");
    gpio_pin_configure(dev, BTN_DEC_PIN, BTN_DEC_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_DEC_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_dec, dec_pressed, BIT(BTN_DEC_PIN));
    gpio_add_callback(dev, &cb_dec);

    printk("[Init] Button control (SW0, SW1, SW2, SW3)\n");
}

/* ============================
 *   ===== LED Control =======
 * ============================
 */

#define LED_NODE_ONOFF    DT_ALIAS(led0)
#define LED_NODE_NORMAL   DT_ALIAS(led1)
#define LED_NODE_LOW      DT_ALIAS(led2)
#define LED_NODE_HIGH     DT_ALIAS(led3)

static K_THREAD_STACK_DEFINE(led_stack, 1024);
static struct k_thread led_thread;

static void led_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *d_onoff  = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_ONOFF, gpios));
    const struct device *d_normal = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_NORMAL, gpios));
    const struct device *d_low    = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_LOW, gpios));
    const struct device *d_high   = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_HIGH, gpios));

    __ASSERT(device_is_ready(d_onoff),  "LED_ONOFF não pronto");
    __ASSERT(device_is_ready(d_normal), "LED_NORMAL não pronto");
    __ASSERT(device_is_ready(d_low),    "LED_LOW não pronto");
    __ASSERT(device_is_ready(d_high),   "LED_HIGH não pronto");

    gpio_pin_configure(d_onoff,  DT_GPIO_PIN(LED_NODE_ONOFF, gpios),  GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_ONOFF, gpios));
    gpio_pin_configure(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios), GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_NORMAL, gpios));
    gpio_pin_configure(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),    GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_LOW, gpios));
    gpio_pin_configure(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),   GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_HIGH, gpios));

    for (;;) {
        bool on = rtdb_get_system_on();
        int16_t cur = rtdb_get_current_temp();
        int16_t sp  = rtdb_get_setpoint();

        /* LED1: sistema ON/OFF */
        gpio_pin_set(d_onoff, DT_GPIO_PIN(LED_NODE_ONOFF, gpios), (int)on);

        if (!on) {
            /* se está desligado, todos os outros LEDs apagam */
            gpio_pin_set(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios), 0);
            gpio_pin_set(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),    0);
            gpio_pin_set(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),   0);
        } else {
            if (cur < sp - 2) {
                /* temperatura muito baixa */
                gpio_pin_set(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),    1);
                gpio_pin_set(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios), 0);
                gpio_pin_set(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),   0);
            } else if (cur > sp + 2) {
                /* temperatura muito alta */
                gpio_pin_set(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),   1);
                gpio_pin_set(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios), 0);
                gpio_pin_set(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),    0);
            } else {
                /* dentro da faixa normal */
                gpio_pin_set(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios), 1);
                gpio_pin_set(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),    0);
                gpio_pin_set(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),   0);
            }
        }
        k_msleep(500);
    }
}

void led_ctrl_init(void)
{
    k_thread_create(&led_thread, led_stack, K_THREAD_STACK_SIZEOF(led_stack),
                    led_task, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    printk("[Init] LED control\n");
}


/* ===== TC74 “Fake” Temperature Sensor (apenas para testes) ===== */

static K_THREAD_STACK_DEFINE(sensor_stack, 1024);
static struct k_thread sensor_thread;

static void sensor_fake_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    int16_t last_temp = INT16_MIN;  /* valor inviável, força atualização logo no primeiro ciclo */
    int16_t fake_temp = 25;

    for (;;) {
        /* --- simula leitura de temperatura fixa (25°C) --- */
        int16_t new_temp = fake_temp;

        /* só atualiza/printa se mudou em relação à última iteração */
        if (new_temp != last_temp) {
            last_temp = new_temp;
            rtdb_set_current_temp(new_temp);
            printk("[SensorFake] current_temp atualizado para %d°C\n", new_temp);
            printk("[RTDB] current_temp atualizado para %d°C\n", new_temp);
        }

        k_msleep(1000);

    }
}

static void tempsensor_init(void)
{
    /* não faz mais binding real de I2C, só cria a thread fake */
    k_thread_create(&sensor_thread, sensor_stack,
                    K_THREAD_STACK_SIZEOF(sensor_stack),
                    sensor_fake_task, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    printk("[Init] SensorFake iniciado\n");
}


// /* ==================== Sensor TC74 via I²C ==================== */

// #define TC74_CMD_RTR   0x00u

// /* vincula o nodelabel “tc74sensor” (definido na overlay) */
// #define I2C0_NID        DT_NODELABEL(tc74sensor)
// static const struct i2c_dt_spec tc74 = I2C_DT_SPEC_GET(I2C0_NID);

// static K_THREAD_STACK_DEFINE(sensor_stack, 1024);
// static struct k_thread sensor_thread;

// static void sensor_task(void *p1, void *p2, void *p3)
// {
//     ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

//     int ret;
//     uint8_t cmd = TC74_CMD_RTR;
//     uint8_t temp_raw;

//     /* Primeiro, escrever “Read TempeRature” (RTR), para posicionar o ponteiro de leitura */
//     ret = i2c_write_dt(&tc74, &cmd, 1);
//     if (ret != 0) {
//         printk("[Sensor] falha no write RTR: %d\n", ret);
//         /* mas continua, vamos tentar de qualquer forma ler */
//     } else {
//         printk("[Sensor] RTR enviado com sucesso\n");
//     }

//     while (1) {
//         /* Leitura do registrador de temperatura (1 byte) */
//         ret = i2c_read_dt(&tc74, &temp_raw, 1);
//         if (ret == 0) {
//             int16_t temp_signed = (int16_t)(int8_t)temp_raw;
//             rtdb_set_current_temp(temp_signed);
//             printk("[Sensor] current_temp lido = %d°C\n", temp_signed);
//         } else {
//             printk("[Sensor] falha no read: %d\n", ret);
//         }
//         k_sleep(K_MSEC(1000));
//     }
// }

// static void tempsensor_init(void)
// {
//     /* Checa se o barramento está pronto */
//     if (!device_is_ready(tc74.bus)) {
//         printk("I2C bus %s nao pronto\n", tc74.bus->name);
//         return;
//     }
//     /* Cria a thread de leitura do sensor */
//     k_thread_create(&sensor_thread, sensor_stack,
//                     K_THREAD_STACK_SIZEOF(sensor_stack),
//                     sensor_task, NULL, NULL, NULL,
//                     5, 0, K_NO_WAIT);
//     printk("[Init] TC74 via I2C OK em %s, addr=0x%02x\n",
//            tc74.bus->name, tc74.addr);
// }

/* ============================
 *       ===== Main ======
 * ============================
 */
int main(void)
{

    print_menu();

    uart_comm_init();
    button_ctrl_init();
    led_ctrl_init();
    tempsensor_init();
    controller_init();

    return 0;
}
