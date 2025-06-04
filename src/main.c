/**
 * @file main.c
 * @brief Thermal Process Controller (versão monolítica)
 *
 * @details
 *   - Botões: liga/desliga sistema, inc/dec setpoint (atualiza RTDB)
 *   - LEDs: indicam estado ON/OFF, “normal”, “baixo” ou “alto” comparando current_temp x setpoint
 *   - Sensor TC74A0 via I²C: escreve comando RTR (0x00) e lê 1 byte (temperatura em °C), atualiza RTDB
 *   - Controlador ON/OFF: liga/desliga MOSFET (p1.12) conforme histerese ±1°C sobre setpoint
 *   - UART: permite consultar current_temp e mudar max_temp/min_temp/sampling rate/on-off via comandos “#…!”
 *
 *   Este ficheiro inicializa todas as tarefas (threads) do sistema:
 *     - UART
 *     - Controlo de botões
 *     - Controlo de LEDs
 *     - Leitura do sensor I²C
 *     - Controlador ON/OFF
 *
 * @author Nuno Tomás Gomes [98807] / Vasco Pestana [88827]
 * @date 04/06/2025
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
 
 #define DEBOUNCE_MS  50  /**< Tempo de debounce para botões (ms) */
 
 /* --------------------------------------------------------------------------
  * Callbacks de GPIO e timestamps para debounce
  * -------------------------------------------------------------------------- */
 static struct gpio_callback cb_onoff;  /**< Callback handler para SW0 (on/off) */
 static struct gpio_callback cb_inc;    /**< Callback handler para SW1 (+setpoint) */
 static struct gpio_callback cb_menu;   /**< Callback handler para SW2 (exibe menu) */
 static struct gpio_callback cb_dec;    /**< Callback handler para SW3 (-setpoint) */
 
 /**
  * @brief Imprime o menu de uso na consola (quando SW2 é pressionado)
  *
  * Detalha quais botões físicos estão associados a cada função e a convenção dos LEDs.
  */
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
            " Comandos UART (115200, 8, n, 1):\n"
            "   • #MxxxYYY! → define max_temp (xxx = 0..999) e envia ack\n"
            "   • #mxxxYYY! → define min_temp (xxx = 0..999) e envia ack\n"
            "   • #C!       → consulta current_temp (responde #cXXXYYY!)\n"
            "   • #E0yyy!   → liga sistema e envia ack\n"
            "   • #E1yyy!   → desliga sistema e envia ack\n"
            "   • #RxxxxYYY!→ define sampling rate em ms (0000..9999)\n"
            "   • #r!       → consulta sampling rate (responde #sXXXXYYY!)\n"
            "   • #S…!      → define parâmetros do controlador (stub) e envia ack\n"
            "\n"
            " Use os botões para controlar ON/OFF e ajustar setpoint.\n"
            "============================================\n");
 }
 
 /**
  * @brief Callback do botão SW0 (ON/OFF) com lógica de debounce
  *
  * @param dev  Ponteiro para o dispositivo GPIO (não utilizado diretamente)
  * @param cb   Ponteiro para a estrutura de callback (não utilizado dentro)
  * @param pins Máscara de pinos que dispararam a interrupção (não usado)
  *
  * Alterna o valor booleano de system_on na RTDB e imprime estado atual.
  */
 static void onoff_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
 {
     ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
 
     /* Debounce: apenas aceita evento se passou mais de DEBOUNCE_MS desde o último */
     static int64_t last_onoff = 0;
     int64_t now = k_uptime_get();
     if (now - last_onoff < DEBOUNCE_MS) {
         return;
     }
     last_onoff = now;
 
     bool on = !rtdb_get_system_on();
     rtdb_set_system_on(on);
     printk("\n[Botão SW0] Sistema agora: %s\n", on ? "ON" : "OFF");
 }
 
 /**
  * @brief Callback do botão SW1 (incrementa setpoint) com debounce e verificação de max_temp
  *
  * @param dev  Ponteiro para o dispositivo GPIO (não utilizado diretamente)
  * @param cb   Ponteiro para a estrutura de callback (não utilizado dentro)
  * @param pins Máscara de pinos que dispararam a interrupção (não usado)
  *
  * Incrementa o setpoint em 1°C e impede ultrapassar max_temp. Informa via printk.
  */
 static void inc_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
 {
     ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
 
     /* Debounce */
     static int64_t last_inc = 0;
     int64_t now = k_uptime_get();
     if (now - last_inc < DEBOUNCE_MS) {
         return;
     }
     last_inc = now;
 
     int16_t tentativa = rtdb_get_setpoint() + 1;
     rtdb_set_setpoint(tentativa);
 
     int16_t real = rtdb_get_setpoint();
     int16_t mx   = rtdb_get_max_temp();
 
     if (real >= mx && tentativa > mx) {
         printk("[Botão SW1] Temperatura máxima atingida (%d °C)\n", mx);
     } else {
         printk("[Botão SW1] Setpoint incrementado para %d °C\n", real);
     }
 }
 
 /**
  * @brief Callback do botão SW2 (imprime menu) com debounce
  *
  * @param dev  Ponteiro para o dispositivo GPIO (não utilizado diretamente)
  * @param cb   Ponteiro para a estrutura de callback (não utilizado dentro)
  * @param pins Máscara de pinos que dispararam a interrupção (não usado)
  *
  * Chama a função print_menu() para mostrar instruções na consola.
  */
 static void menu_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
 {
     ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
 
     /* Debounce */
     static int64_t last_menu = 0;
     int64_t now = k_uptime_get();
     if (now - last_menu < DEBOUNCE_MS) {
         return;
     }
     last_menu = now;
 
     print_menu();
 }
 
 /**
  * @brief Callback do botão SW3 (decrementa setpoint) com debounce e verificação de min_temp
  *
  * @param dev  Ponteiro para o dispositivo GPIO (não utilizado diretamente)
  * @param cb   Ponteiro para a estrutura de callback (não utilizado dentro)
  * @param pins Máscara de pinos que dispararam a interrupção (não usado)
  *
  * Decrementa o setpoint em 1°C e impede descer abaixo de min_temp. Informa via printk.
  */
 static void dec_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
 {
     ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
 
     /* Debounce */
     static int64_t last_dec = 0;
     int64_t now = k_uptime_get();
     if (now - last_dec < DEBOUNCE_MS) {
         return;
     }
     last_dec = now;
 
     int16_t tentativa = rtdb_get_setpoint() - 1;
     rtdb_set_setpoint(tentativa);
 
     int16_t real = rtdb_get_setpoint();
     int16_t mi   = rtdb_get_min_temp();
 
     if (real <= mi && tentativa < mi) {
         printk("[Botão SW3] Temperatura mínima atingida (%d °C)\n", mi);
     } else {
         printk("[Botão SW3] Setpoint decrementado para %d °C\n", real);
     }
 }
 
 /**
  * @brief Inicializa todos os botões (SW0..SW3) com configurações de GPIO e callbacks
  *
  * Configura cada botão como entrada e ativa interrupção no flanco de subida. Liga callbacks
  * onoff_pressed, inc_pressed, menu_pressed e dec_pressed.
  */
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
 
 /* =========================
  *  ===== LED Control =====
  * =========================
 */
 
 #define LED_NODE_ONOFF    DT_ALIAS(led0)
 #define LED_NODE_NORMAL   DT_ALIAS(led1)
 #define LED_NODE_LOW      DT_ALIAS(led2)
 #define LED_NODE_HIGH     DT_ALIAS(led3)
 
 static K_THREAD_STACK_DEFINE(led_stack, 1024);  
 static struct k_thread led_thread;               
 
 /**
  * @brief Tarefa que atualiza o estado dos LEDs em loop contínuo
  *
  * - LED0: indica se system_on está ativo
  * - LED1: temperatura “normal” (|cur – sp| ≤ 2°C)
  * - LED2: temperatura “abaixo” (cur < sp – 2°C)
  * - LED3: temperatura “acima” (cur > sp + 2°C)
  *
  * Esta função lê periodicamente (a cada 500 ms) os valores na RTDB:
  *   - system_on
  *   - current_temp
  *   - setpoint
  *
  * E define os pinos GPIO correspondentes para acender/apagar cada LED.
  *
  * @param p1  Não utilizado
  * @param p2  Não utilizado
  * @param p3  Não utilizado
  */
 static void led_task(void *p1, void *p2, void *p3)
 {
     ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
 
     const struct device *d_onoff  = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_ONOFF, gpios));
     const struct device *d_normal = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_NORMAL, gpios));
     const struct device *d_low    = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_LOW, gpios));
     const struct device *d_high   = DEVICE_DT_GET(DT_GPIO_CTLR(LED_NODE_HIGH, gpios));
 
     __ASSERT(device_is_ready(d_onoff),  "LED_ONOFF não pronto");
     __ASSERT(device_is_ready(d_normal), "LED_NORMAL não pronto");
     __ASSERT(device_is_ready(d_low),    "LED_LOW não pronto");
     __ASSERT(device_is_ready(d_high),   "LED_HIGH não pronto");
 
     gpio_pin_configure(d_onoff,  DT_GPIO_PIN(LED_NODE_ONOFF, gpios),
                        GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_ONOFF, gpios));
     gpio_pin_configure(d_normal, DT_GPIO_PIN(LED_NODE_NORMAL, gpios),
                        GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_NORMAL, gpios));
     gpio_pin_configure(d_low,    DT_GPIO_PIN(LED_NODE_LOW, gpios),
                        GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_LOW, gpios));
     gpio_pin_configure(d_high,   DT_GPIO_PIN(LED_NODE_HIGH, gpios),
                        GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_HIGH, gpios));
 
     for (;;) {
         bool on = rtdb_get_system_on();
         int16_t cur = rtdb_get_current_temp();
         int16_t sp  = rtdb_get_setpoint();
 
         /* LED0: sistema ON/OFF */
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
 
 /**
  * @brief Inicializa o controlo de LEDs criando a thread led_task
  */
 void led_ctrl_init(void)
 {
     k_thread_create(&led_thread, led_stack, K_THREAD_STACK_SIZEOF(led_stack),
                     led_task, NULL, NULL, NULL,
                     5, 0, K_NO_WAIT);
     printk("[Init] LED control\n");
 }
 
 /* ==================== Sensor TC74 via I²C ==================== */
 
 #define TC74_CMD_RTR   0x00u  
 #define I2C0_NID        DT_NODELABEL(tc74sensor)  
 
 static const struct i2c_dt_spec tc74 = I2C_DT_SPEC_GET(I2C0_NID);  
 
 static K_THREAD_STACK_DEFINE(sensor_stack, 1024);  
 static struct k_thread sensor_thread;               
 
 /**
  * @brief Tarefa que lê continuamente a temperatura do TC74 e atualiza a RTDB
  *
  *   - No arranque, envia o comando RTR (0x00) para posicionar o ponteiro
  *   - Em cada ciclo (delay = sampling_rate), escreve RTR e faz i2c_read_dt(&temp_raw,1)
  *   - Converte o byte lido (complemento a dois) para int16_t e chama rtdb_set_current_temp()
  *
  * @param p1  Não utilizado
  * @param p2  Não utilizado
  * @param p3  Não utilizado
  */
 static void sensor_task(void *p1, void *p2, void *p3)
 {
     ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
 
     int ret;
     uint8_t cmd = TC74_CMD_RTR;
     uint8_t temp_raw;
 
     /* Primeiro, escrever “Read Temperature Register” (RTR), para posicionar o ponteiro */
     ret = i2c_write_dt(&tc74, &cmd, 1);
     if (ret != 0) {
         printk("[Sensor] falha no write RTR: %d\n", ret);
         /* Continua, mas possivelmente aponta ponteiro incorretamente */
     } else {
         printk("[Sensor] RTR enviado com sucesso\n");
     }
 
     while (1) {
         /* Antes de cada leitura, reposiciona o ponteiro para o registro de temperatura */
         cmd = TC74_CMD_RTR;
         ret = i2c_write_dt(&tc74, &cmd, 1);
         if (ret != 0) {
             printk("[Sensor] falha no write RTR (loop): %d\n", ret);
         }
 
         /* Leitura do registrador de temperatura (1 byte) */
         ret = i2c_read_dt(&tc74, &temp_raw, 1);
         if (ret == 0) {
             int16_t temp_signed = (int16_t)(int8_t)temp_raw;
             rtdb_set_current_temp(temp_signed);
             printk("[Sensor] current_temp lido = %d°C\n", temp_signed);
         } else {
             printk("[Sensor] falha no read: %d\n", ret);
         }
 
         uint32_t delay = rtdb_get_sampling_rate();
         k_msleep(delay);
     }
 }
 
 /**
  * @brief Inicializa o sensor TC74 criando a thread sensor_task
  *
  * Verifica se o barramento I²C está pronto e, se estiver, cria a thread de leitura.
  */
 static void tempsensor_init(void)
 {
     if (!device_is_ready(tc74.bus)) {
         printk("I2C bus %s não pronto\n", tc74.bus->name);
         return;
     }
     k_thread_create(&sensor_thread, sensor_stack,
                     K_THREAD_STACK_SIZEOF(sensor_stack),
                     sensor_task, NULL, NULL, NULL,
                     5, 0, K_NO_WAIT);
     printk("[Init] TC74 via I2C OK em %s, addr=0x%02x\n",
            tc74.bus->name, tc74.addr);
 }
 
 /**
  * @brief Função principal (entry point) do firmware
  *
  *   - Exibe menu inicial
  *   - Inicializa todas as tarefas do sistema:
  *       • uart_comm_init(): thread de comunicação UART
  *       • button_ctrl_init(): configuração de botões e callbacks
  *       • led_ctrl_init(): thread de controlo de LEDs
  *       • tempsensor_init(): thread de leitura do sensor I²C
  *       • controller_init(): thread do controlador ON/OFF do aquecedor
  *
  * @return Nunca retorna (ainda que a função devolva 0, o Zephyr mantém as threads vivas)
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
 
