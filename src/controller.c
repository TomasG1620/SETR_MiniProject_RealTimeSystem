/**
 * @file controller.c
 * @brief On/off controller for thermal process
 * @details Simple hysteresis controller: reads setpoint and current_temp from RTDB
 *          and drives heater via PWM (full on/off). Compliant with MISRA-C.
 */

#include "controller.h"
#include "rtdb.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* Heater PWM alias in DT */
#define HEATER_NODE        DT_ALIAS(pwm_led0)
#define HEATER_CTLR_NODE   DT_PWMS_CTLR(HEATER_NODE)
#define HEATER_CHANNEL     DT_PWMS_CHANNEL(HEATER_NODE)
#define HEATER_FLAGS       DT_PWMS_FLAGS(HEATER_NODE)

/* PWM period in nanoseconds (1 kHz) */
#define PWM_PERIOD_NS      1000000U

/* Hysteresis band (°C) */
#define HYSTERESIS         1

/* Control task parameters */
#define CTRL_STACK_SIZE    1024U
#define CTRL_PRIORITY      5U
static K_THREAD_STACK_DEFINE(ctrl_stack, CTRL_STACK_SIZE);
static struct k_thread ctrl_thread;

/**
 * @brief Control loop: on/off with hysteresis
 */
static void control_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *pwm_dev = DEVICE_DT_GET(HEATER_CTLR_NODE);
    __ASSERT(pwm_dev != NULL, "PWM controller device not found");

    bool heater_on = false;

    for (;;) {
        int16_t setpoint = rtdb_get_setpoint();
        int16_t current  = rtdb_get_current_temp();

        if (!heater_on) {
            if (current < (int16_t)(setpoint - HYSTERESIS)) {
                heater_on = true;
            }
        } else {
            if (current > (int16_t)(setpoint + HYSTERESIS)) {
                heater_on = false;
            }
        }

        /* Drive heater: full on/off via PWM */
        if (heater_on) {
            (void)pwm_set(pwm_dev, HEATER_CHANNEL,
                          PWM_PERIOD_NS, PWM_PERIOD_NS,
                          HEATER_FLAGS);
        } else {
            (void)pwm_set(pwm_dev, HEATER_CHANNEL,
                          PWM_PERIOD_NS, 0U,
                          HEATER_FLAGS);
        }

        printk("[Controller] sp=%d, cur=%d, heater=%d\n",
               setpoint, current, (int)heater_on);

        k_sleep(K_SECONDS(30));
    }
}

/**
 * @brief Initialize controller
 */
void controller_init(void)
{
    k_thread_create(&ctrl_thread, ctrl_stack, CTRL_STACK_SIZE,
                    control_task, NULL, NULL, NULL,
                    CTRL_PRIORITY, 0, K_NO_WAIT);
    printk("Controller initialized\n");
}
