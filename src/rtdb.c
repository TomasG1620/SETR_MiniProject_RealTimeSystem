#include "rtdb.h"
#include <zephyr/kernel.h>

static rtdb_t g_rtdb = {
    .system_on    = false,
    .setpoint     = 25,
    .current_temp = 0,
    .max_temp     = 80,
    .min_temp     = 0
};

static struct k_mutex rtdb_mutex;

static int rtdb_mutex_init(const struct device *dev)
{
    ARG_UNUSED(dev);
    k_mutex_init(&rtdb_mutex);
    return 0;
}
SYS_INIT(rtdb_mutex_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

bool rtdb_get_system_on(void)
{
    bool v;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    v = g_rtdb.system_on;
    k_mutex_unlock(&rtdb_mutex);
    return v;
}

void rtdb_set_system_on(bool on)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.system_on = on;
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_setpoint(void)
{
    int16_t v;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    v = g_rtdb.setpoint;
    k_mutex_unlock(&rtdb_mutex);
    return v;
}

void rtdb_set_setpoint(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    if (val > g_rtdb.max_temp) {
        g_rtdb.setpoint = g_rtdb.max_temp;
    } else if (val < g_rtdb.min_temp) {
        g_rtdb.setpoint = g_rtdb.min_temp;
    } else {
        g_rtdb.setpoint = val;
    }
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_current_temp(void)
{
    int16_t v;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    v = g_rtdb.current_temp;
    k_mutex_unlock(&rtdb_mutex);
    return v;
}

void rtdb_set_current_temp(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.current_temp = val;
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_max_temp(void)
{
    int16_t v;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    v = g_rtdb.max_temp;
    k_mutex_unlock(&rtdb_mutex);
    return v;
}

void rtdb_set_max_temp(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.max_temp = val;
    if (g_rtdb.setpoint > g_rtdb.max_temp) {
        g_rtdb.setpoint = g_rtdb.max_temp;
    }
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_min_temp(void)
{
    int16_t v;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    v = g_rtdb.min_temp;
    k_mutex_unlock(&rtdb_mutex);
    return v;
}

void rtdb_set_min_temp(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.min_temp = val;
    if (g_rtdb.setpoint < g_rtdb.min_temp) {
        g_rtdb.setpoint = g_rtdb.min_temp;
    }
    k_mutex_unlock(&rtdb_mutex);
}
