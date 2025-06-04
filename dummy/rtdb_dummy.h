#ifndef RTDB_DUMMY_H
#define RTDB_DUMMY_H

#include <stdint.h>
#include <stdbool.h>

/* Semelhante ao original */
typedef struct {
    bool     system_on;
    int16_t  setpoint;
    int16_t  current_temp;
    int16_t  max_temp;
    int16_t  min_temp;
    bool     heater;
    uint32_t sampling_rate_ms;
} rtdb_dummy_t;

/* Inicializa todos os valores para default */
void rtdb_dummy_init(void);

/* Get / set de system_on */
bool     rtdb_dummy_get_system_on(void);
void     rtdb_dummy_set_system_on(bool on);

/* Get / set de setpoint (respeitando min/max) */
int16_t  rtdb_dummy_get_setpoint(void);
void     rtdb_dummy_set_setpoint(int16_t val);

/* Get / set de current_temp */
int16_t  rtdb_dummy_get_current_temp(void);
void     rtdb_dummy_set_current_temp(int16_t val);

/* Get / set de max_temp (respeitando setpoint) */
int16_t  rtdb_dummy_get_max_temp(void);
void     rtdb_dummy_set_max_temp(int16_t val);

/* Get / set de min_temp (respeitando setpoint) */
int16_t  rtdb_dummy_get_min_temp(void);
void     rtdb_dummy_set_min_temp(int16_t val);

/* Get / set de heater (para testes de controller) */
bool     rtdb_dummy_get_heater(void);
void     rtdb_dummy_set_heater(bool on);

/* Get / set de sampling_rate_ms (entre 10 e 60000) */
uint32_t rtdb_dummy_get_sampling_rate(void);
void     rtdb_dummy_set_sampling_rate(uint32_t ms);

#endif /* RTDB_DUMMY_H */

