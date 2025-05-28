/**
 * @file main.c
 * @brief Thermal Process Controller - Monolithic implementation
 * @details Inline implementations of button, LED, and temperature sensor modules.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

#include "rtdb.h"
#include "uartcomm.h"
#include "controller.h"

/* ===== Button Control (inline) ===== */
#define BTN_NODE_ONOFF   DT_ALIAS(sw0)   /* S1 */
#define BTN_NODE_INC     DT_ALIAS(sw1)   /* S2 */
#define BTN_NODE_DEC     DT_ALIAS(sw3)   /* S4 */

#define BTN_ONOFF_DEV    DT_GPIO_CTLR(BTN_NODE_ONOFF, gpios)
#define BTN_ONOFF_PIN    DT_GPIO_PIN(BTN_NODE_ONOFF, gpios)
#define BTN_ONOFF_FLAGS  (DT_GPIO_FLAGS(BTN_NODE_ONOFF, gpios) | GPIO_INPUT)

#define BTN_INC_DEV      DT_GPIO_CTLR(BTN_NODE_INC, gpios)
#define BTN_INC_PIN      DT_GPIO_PIN(BTN_NODE_INC, gpios)
#define BTN_INC_FLAGS    (DT_GPIO_FLAGS(BTN_NODE_INC, gpios) | GPIO_INPUT)

#define BTN_DEC_DEV      DT_GPIO_CTLR(BTN_NODE_DEC, gpios)
#define BTN_DEC_PIN      DT_GPIO_PIN(BTN_NODE_DEC, gpios)
#define BTN_DEC_FLAGS    (DT_GPIO_FLAGS(BTN_NODE_DEC, gpios) | GPIO_INPUT)

static struct gpio_callback cb_onoff;
static struct gpio_callback cb_inc;
static struct gpio_callback cb_dec;

static void onoff_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    bool on = !rtdb_get_system_on();
    rtdb_set_system_on(on);
    printk("[Button] ON/OFF toggled -> %d\n", on);
}

static void inc_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    int16_t sp = rtdb_get_setpoint() + 1;
    rtdb_set_setpoint(sp);
    printk("[Button] Setpoint increased -> %d°C\n", sp);
}

static void dec_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    int16_t sp = rtdb_get_setpoint() - 1;
    rtdb_set_setpoint(sp);
    printk("[Button] Setpoint decreased -> %d°C\n", sp);
}

void button_ctrl_init(void)
{
    const struct device *dev;

    /* ON/OFF */
    dev = DEVICE_DT_GET(BTN_ONOFF_DEV);
    gpio_pin_configure(dev, BTN_ONOFF_PIN, BTN_ONOFF_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_ONOFF_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_onoff, onoff_pressed, BIT(BTN_ONOFF_PIN));
    gpio_add_callback(dev, &cb_onoff);

    /* INC */
    dev = DEVICE_DT_GET(BTN_INC_DEV);
    gpio_pin_configure(dev, BTN_INC_PIN, BTN_INC_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_INC_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_inc, inc_pressed, BIT(BTN_INC_PIN));
    gpio_add_callback(dev, &cb_inc);

    /* DEC */
    dev = DEVICE_DT_GET(BTN_DEC_DEV);
    gpio_pin_configure(dev, BTN_DEC_PIN, BTN_DEC_FLAGS);
    gpio_pin_interrupt_configure(dev, BTN_DEC_PIN, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&cb_dec, dec_pressed, BIT(BTN_DEC_PIN));
    gpio_add_callback(dev, &cb_dec);

    printk("[Init] Button control initialized\n");
}

/* ===== LED Control (inline) ===== */
#define LED_NODE_ONOFF    DT_ALIAS(led0)   /* LED1 */
#define LED_NODE_NORMAL   DT_ALIAS(led1)   /* LED2 */
#define LED_NODE_LOW      DT_ALIAS(led2)   /* LED3 */
#define LED_NODE_HIGH     DT_ALIAS(led3)   /* LED4 */

#define LED_ONOFF_DEV     DT_GPIO_CTLR(LED_NODE_ONOFF, gpios)
#define LED_ONOFF_PIN     DT_GPIO_PIN(LED_NODE_ONOFF, gpios)
#define LED_ONOFF_FLAGS   GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(LED_NODE_ONOFF, gpios)

#define LED_NORMAL_DEV    DT_GPIO_CTLR(LED_NODE_NORMAL, gpios)
#define LED_NORMAL_PIN    DT_GPIO_PIN(LED_NODE_NORMAL, gpios)
#define LED_NORMAL_FLAGS  GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(LED_NODE_NORMAL, gpios)

#define LED_LOW_DEV       DT_GPIO_CTLR(LED_NODE_LOW, gpios)
#define LED_LOW_PIN       DT_GPIO_PIN(LED_NODE_LOW, gpios)
#define LED_LOW_FLAGS     GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(LED_NODE_LOW, gpios)

#define LED_HIGH_DEV      DT_GPIO_CTLR(LED_NODE_HIGH, gpios)
#define LED_HIGH_PIN      DT_GPIO_PIN(LED_NODE_HIGH, gpios)
#define LED_HIGH_FLAGS    GPIO_OUTPUT_ACTIVE | DT_GPIO_FLAGS(LED_NODE_HIGH, gpios)

#define LED_STACK_SIZE    1024U
#define LED_PRIORITY      5U
static K_THREAD_STACK_DEFINE(led_stack, LED_STACK_SIZE);
static struct k_thread led_thread;

static void led_task(void *p1, void *p2, void *p3)
{
    const struct device *dev;
    dev = DEVICE_DT_GET(LED_ONOFF_DEV);
    gpio_pin_configure(dev, LED_ONOFF_PIN, GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_ONOFF, gpios));
    dev = DEVICE_DT_GET(LED_NORMAL_DEV);
    gpio_pin_configure(dev, LED_NORMAL_PIN, GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_NORMAL, gpios));
    dev = DEVICE_DT_GET(LED_LOW_DEV);
    gpio_pin_configure(dev, LED_LOW_PIN, GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_LOW, gpios));
    dev = DEVICE_DT_GET(LED_HIGH_DEV);
    gpio_pin_configure(dev, LED_HIGH_PIN, GPIO_OUTPUT_INACTIVE | DT_GPIO_FLAGS(LED_NODE_HIGH, gpios));

    for (;;) {
        bool on = rtdb_get_system_on();
        int16_t cur = rtdb_get_current_temp();
        int16_t sp  = rtdb_get_setpoint();

        /* System on/off LED */
        gpio_pin_set(DEVICE_DT_GET(LED_ONOFF_DEV), LED_ONOFF_PIN, (int)on);
        if (!on) {
            gpio_pin_set(DEVICE_DT_GET(LED_NORMAL_DEV), LED_NORMAL_PIN, 0);
            gpio_pin_set(DEVICE_DT_GET(LED_LOW_DEV), LED_LOW_PIN, 0);
            gpio_pin_set(DEVICE_DT_GET(LED_HIGH_DEV), LED_HIGH_PIN, 0);
        } else if (cur < sp - 1) {
            gpio_pin_set(DEVICE_DT_GET(LED_LOW_DEV), LED_LOW_PIN, 1);
            gpio_pin_set(DEVICE_DT_GET(LED_NORMAL_DEV), LED_NORMAL_PIN, 0);
            gpio_pin_set(DEVICE_DT_GET(LED_HIGH_DEV), LED_HIGH_PIN, 0);
        } else if (cur > sp + 1) {
            gpio_pin_set(DEVICE_DT_GET(LED_HIGH_DEV), LED_HIGH_PIN, 1);
            gpio_pin_set(DEVICE_DT_GET(LED_NORMAL_DEV), LED_NORMAL_PIN, 0);
            gpio_pin_set(DEVICE_DT_GET(LED_LOW_DEV), LED_LOW_PIN, 0);
        } else {
            gpio_pin_set(DEVICE_DT_GET(LED_NORMAL_DEV), LED_NORMAL_PIN, 1);
            gpio_pin_set(DEVICE_DT_GET(LED_LOW_DEV), LED_LOW_PIN, 0);
            gpio_pin_set(DEVICE_DT_GET(LED_HIGH_DEV), LED_HIGH_PIN, 0);
        }
        k_sleep(K_MSEC(1000));
    }
}

void led_ctrl_init(void)
{
    k_thread_create(&led_thread, led_stack, LED_STACK_SIZE,
                    led_task, NULL, NULL, NULL,
                    LED_PRIORITY, 0, K_NO_WAIT);
    printk("[Init] LED control initialized\n");
}

/* ===== Temperature Sensor (inline) ===== */
#define SENSOR_STACK_SIZE 1024U
#define SENSOR_PRIORITY   5U
static K_THREAD_STACK_DEFINE(sensor_stack, SENSOR_STACK_SIZE);
static struct k_thread sensor_thread;

static void sensor_task(void *p1, void *p2, void *p3)
{
    int16_t fake_temp = rtdb_get_setpoint();
    bool increasing = true;

    for (;;) {
        int16_t sp = rtdb_get_setpoint();
        if (increasing) {
            fake_temp++;
            if (fake_temp >= sp + 10) increasing = false;
        } else {
            fake_temp--;
            if (fake_temp <= sp - 10) increasing = true;
        }
        rtdb_set_current_temp(fake_temp);
        printk("[Sensor] current_temp = %d°C\n", fake_temp);
        k_sleep(K_SECONDS(5));
    }
}

void temp_sensor_init(void)
{
    k_thread_create(&sensor_thread, sensor_stack, SENSOR_STACK_SIZE,
                    sensor_task, NULL, NULL, NULL,
                    SENSOR_PRIORITY, 0, K_NO_WAIT);
    printk("[Init] Temp sensor initialized\n");
}

/* ===== Main ===== */
int main(void)
{
    uart_comm_init();
    button_ctrl_init();
    led_ctrl_init();
    temp_sensor_init();
    controller_init();
    return 0;
}
