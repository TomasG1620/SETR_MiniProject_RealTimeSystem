/**
 * @file rtdb.c
 * @brief Real-Time Database implementation
 * @details Thread-safe getters and setters for the RTDB, using Zephyr mutex
 */

#include "rtdb.h"
#include <zephyr/kernel.h>

/* Static instance of the RTDB and its mutex */
static rtdb_t g_rtdb = {
    .system_on = false,
    .setpoint = 25,
    .current_temp = 0,
    .max_temp = 50
};

static struct k_mutex rtdb_mutex;

/**
 * @brief Initialize RTDB mutex at system startup
 */
static int rtdb_mutex_init(void)
{
    k_mutex_init(&rtdb_mutex);
    return 0;
}
SYS_INIT(rtdb_mutex_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

bool rtdb_get_system_on(void)
{
    bool val;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    val = g_rtdb.system_on;
    k_mutex_unlock(&rtdb_mutex);
    return val;
}

void rtdb_set_system_on(bool on)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.system_on = on;
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_setpoint(void)
{
    int16_t val;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    val = g_rtdb.setpoint;
    k_mutex_unlock(&rtdb_mutex);
    return val;
}

void rtdb_set_setpoint(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.setpoint = val;
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_current_temp(void)
{
    int16_t val;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    val = g_rtdb.current_temp;
    k_mutex_unlock(&rtdb_mutex);
    return val;
}

void rtdb_set_current_temp(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.current_temp = val;
    k_mutex_unlock(&rtdb_mutex);
}

int16_t rtdb_get_max_temp(void)
{
    int16_t val;
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    val = g_rtdb.max_temp;
    k_mutex_unlock(&rtdb_mutex);
    return val;
}

void rtdb_set_max_temp(int16_t val)
{
    k_mutex_lock(&rtdb_mutex, K_FOREVER);
    g_rtdb.max_temp = val;
    k_mutex_unlock(&rtdb_mutex);
}
