#ifndef CONTROLLER_DUMMY_H
#define CONTROLLER_DUMMY_H

#include <stdbool.h>
#include <stdint.h>

/**
 * controller_dummy_compute:
 *  - system_on: se falso, retorna false (heater OFF)
 *  - setpoint: temperatura desejada
 *  - current_temp: temperatura atual
 *  - min_temp / max_temp: limites 
 *  - heater_was_on: estado anterior do heater 
 *
 * Retorna: true = liga heater, false = desliga heater
 */
bool controller_dummy_compute(bool system_on,
                              int16_t setpoint,
                              int16_t current_temp,
                              int16_t min_temp,
                              int16_t max_temp,
                              bool heater_was_on);

#endif /* CONTROLLER_DUMMY_H */

