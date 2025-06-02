/**
 * @file controller.c
 * @brief On/off controller for thermal process
 * @details Reads setpoint and current temperature from RTDB,
 *          drives heater via GPIO (active-low MOSFET gate). Compliant with MISRA-C.
 */

#include "controller.h"
#include "rtdb.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* Heater output GPIO configuration */
#define HEATER_GPIO_NODE DT_NODELABEL(gpio1)  /* Use Port 1 to avoid SW1 conflict */
#define HEATER_PIN       12U                  /* P1.12 connected to MOSFET gate */

static const struct device *heater_dev;
static K_THREAD_STACK_DEFINE(ctrl_stack, 1024);
static struct k_thread ctrl_thread;

/**
 * @brief On/off control loop
 *
 * Heater ON when current_temp < setpoint, OFF otherwise.
 * Note: MOSFET gate is active-low: drive pin low to turn heater ON.
 */
static void control_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    for (;;)
    {
        const int16_t sp  = rtdb_get_setpoint();
        const int16_t cur = rtdb_get_current_temp();
        const bool req_on = (cur < sp);

        /* Active-low gate: 0 = ON, 1 = OFF */
        /* Active-high gate: 1 = ON, 0 = OFF */
        gpio_pin_set(heater_dev, HEATER_PIN, req_on ? 1 : 0);

        printk("[Ctrl] sp=%d°C cur=%d°C heater=%d\n", sp, cur, (int)req_on);
        k_sleep(K_MSEC(5000));
    }
}

/**
 * @brief Initialize on/off controller
 */
void controller_init(void)
{
    heater_dev = DEVICE_DT_GET(HEATER_GPIO_NODE);
    __ASSERT(heater_dev != NULL, "Heater GPIO device not found");
    if (!device_is_ready(heater_dev)) {
        printk("[Ctrl] Heater GPIO not ready\n");
        return;
    }

    /* Configure P1.12 as output, default OFF */
    gpio_pin_configure(heater_dev, HEATER_PIN, GPIO_OUTPUT_INACTIVE);
    gpio_pin_set(heater_dev, HEATER_PIN, 1);

    /* Launch control thread */
    k_thread_create(&ctrl_thread, ctrl_stack, K_THREAD_STACK_SIZEOF(ctrl_stack),
                    control_task, NULL, NULL, NULL,
                    5, 0, K_NO_WAIT);
    printk("[Init] Controller\n");
}
