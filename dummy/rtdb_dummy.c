#include "rtdb_dummy.h"
#include <stdlib.h>

static rtdb_dummy_t g_rtdb_dummy;

/* Valor default definido no ficheiro rtdb.c "real" */
void rtdb_dummy_init(void)
{
    g_rtdb_dummy.system_on        = true;
    g_rtdb_dummy.setpoint         = 26;
    g_rtdb_dummy.current_temp     = 0;
    g_rtdb_dummy.max_temp         = 80;
    g_rtdb_dummy.min_temp         = 20;
    g_rtdb_dummy.heater           = false;
    g_rtdb_dummy.sampling_rate_ms = 1000;
}

/* system_on */
bool rtdb_dummy_get_system_on(void)
{
    return g_rtdb_dummy.system_on;
}
void rtdb_dummy_set_system_on(bool on)
{
    g_rtdb_dummy.system_on = on;
}

/* setpoint (entre min_temp e max_temp) */
int16_t rtdb_dummy_get_setpoint(void)
{
    return g_rtdb_dummy.setpoint;
}
void rtdb_dummy_set_setpoint(int16_t val)
{
    if (val > g_rtdb_dummy.max_temp) {
        g_rtdb_dummy.setpoint = g_rtdb_dummy.max_temp;
    } else if (val < g_rtdb_dummy.min_temp) {
        g_rtdb_dummy.setpoint = g_rtdb_dummy.min_temp;
    } else {
        g_rtdb_dummy.setpoint = val;
    }
}

/* current_temp */
int16_t rtdb_dummy_get_current_temp(void)
{
    return g_rtdb_dummy.current_temp;
}
void rtdb_dummy_set_current_temp(int16_t val)
{
    g_rtdb_dummy.current_temp = val;
}

/* max_temp (ajusta setpoint se necessário) */
int16_t rtdb_dummy_get_max_temp(void)
{
    return g_rtdb_dummy.max_temp;
}
void rtdb_dummy_set_max_temp(int16_t val)
{
    g_rtdb_dummy.max_temp = val;
    if (g_rtdb_dummy.setpoint > g_rtdb_dummy.max_temp) {
        g_rtdb_dummy.setpoint = g_rtdb_dummy.max_temp;
    }
}

/* min_temp (ajusta setpoint se necessário) */
int16_t rtdb_dummy_get_min_temp(void)
{
    return g_rtdb_dummy.min_temp;
}
void rtdb_dummy_set_min_temp(int16_t val)
{
    g_rtdb_dummy.min_temp = val;
    if (g_rtdb_dummy.setpoint < g_rtdb_dummy.min_temp) {
        g_rtdb_dummy.setpoint = g_rtdb_dummy.min_temp;
    }
}

/* heater */
bool rtdb_dummy_get_heater(void)
{
    return g_rtdb_dummy.heater;
}
void rtdb_dummy_set_heater(bool on)
{
    g_rtdb_dummy.heater = on;
}

/* sampling_rate_ms (entre 10 e 60000) */
uint32_t rtdb_dummy_get_sampling_rate(void)
{
    return g_rtdb_dummy.sampling_rate_ms;
}
void rtdb_dummy_set_sampling_rate(uint32_t ms)
{
    if (ms < 10) {
        g_rtdb_dummy.sampling_rate_ms = 10U;
    } else if (ms > 60000U) {
        g_rtdb_dummy.sampling_rate_ms = 60000U;
    } else {
        g_rtdb_dummy.sampling_rate_ms = ms;
    }
}

