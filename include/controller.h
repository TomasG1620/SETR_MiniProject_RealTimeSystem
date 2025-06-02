#ifndef CONTROLLER_H
#define CONTROLLER_H

/**
 * @file controller.h
 * @brief On/off controller interface for thermal process
 * @details Provides initialization of the hysteresis-based heater controller
 */

/**
 * @brief Initialize the on/off heater controller
 *
 * This function spawns a Zephyr thread that periodically reads the setpoint and
 * current temperature from the RTDB and drives the heater via PWM with hysteresis.
 */
void controller_init(void);

#endif /* CONTROLLER_H */
