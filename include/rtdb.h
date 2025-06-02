#ifndef RTDB_H
#define RTDB_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    bool    system_on;
    int16_t setpoint;
    int16_t current_temp;
    int16_t max_temp;
    int16_t min_temp;
} rtdb_t;

bool    rtdb_get_system_on(void);
void    rtdb_set_system_on(bool on);

int16_t rtdb_get_setpoint(void);
void    rtdb_set_setpoint(int16_t val);

int16_t rtdb_get_current_temp(void);
void    rtdb_set_current_temp(int16_t val);

int16_t rtdb_get_max_temp(void);
void    rtdb_set_max_temp(int16_t val);

int16_t rtdb_get_min_temp(void);
void    rtdb_set_min_temp(int16_t val);

#endif /* RTDB_H */
