#include "controller_dummy.h"

bool controller_dummy_compute(bool system_on,
                              int16_t setpoint,
                              int16_t current_temp,
                              int16_t min_temp,
                              int16_t max_temp,
                              bool heater_was_on)
{
    (void)heater_was_on; 

    /* 1) Sistema desligado → heater OFF */
    if (!system_on) {
        return false;
    }
    /* 2) Se estiver acima do max_temp → OFF */
    if (current_temp > max_temp) {
        return false;
    }
    /* 3) Se estiver abaixo do min_temp → ON */
    if (current_temp < min_temp) {
        return true;
    }
    /* 4) Normal: se estiver abaixo do setpoint → ON */
    if (current_temp < setpoint) {
        return true;
    }
    /* 5) Caso contrário (>= setpoint) → OFF */
    return false;
}

