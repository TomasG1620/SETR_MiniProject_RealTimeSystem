// tests/test_rtdb.c
#include "unity.h"
#include "rtdb.h"

/* Executado antes de cada teste: zera o RTDB para valores default */
void setUp(void) {
    rtdb_init_default();
}

void tearDown(void) {
    /* Nada a limpar */
}

/* 1) Testa estado inicial (system_on false) */
void test_initial_system_off(void) {
    TEST_ASSERT_FALSE(rtdb_get_system_on());
}

/* 2) Testa toggling system_on */
void test_toggle_system_on_off(void) {
    rtdb_set_system_on(true);
    TEST_ASSERT_TRUE(rtdb_get_system_on());
    rtdb_set_system_on(false);
    TEST_ASSERT_FALSE(rtdb_get_system_on());
}

/* 3) Testa default setpoint / limites de min/max */
void test_default_setpoint_and_limits(void) {
    // Valores default vindo de rtdb_init_default(): setpoint=25, max=80, min=0
    TEST_ASSERT_EQUAL_INT16(25, rtdb_get_setpoint());
    TEST_ASSERT_EQUAL_INT16(80, rtdb_get_max_temp());
    TEST_ASSERT_EQUAL_INT16(0 , rtdb_get_min_temp());
}

/* 4) Testa set_max_temp: se setpoint > max → setpoint fica em max */
void test_setpoint_respects_max(void) {
    rtdb_set_max_temp(30);
    rtdb_set_setpoint(50);
    TEST_ASSERT_EQUAL_INT16(30, rtdb_get_setpoint());  // 50 > max(30)
    rtdb_set_setpoint(30);
    TEST_ASSERT_EQUAL_INT16(30, rtdb_get_setpoint());
}

/* 5) Testa set_min_temp: se setpoint < min → setpoint fica em min */
void test_setpoint_respects_min(void) {
    rtdb_set_min_temp(10);
    rtdb_set_setpoint(5);
    TEST_ASSERT_EQUAL_INT16(10, rtdb_get_setpoint());  // 5 < min(10)
    rtdb_set_setpoint(10);
    TEST_ASSERT_EQUAL_INT16(10, rtdb_get_setpoint());
}

/* 6) Testa current_temp getter/setter */
void test_current_temp_set_get(void) {
    rtdb_set_current_temp(22);
    TEST_ASSERT_EQUAL_INT16(22, rtdb_get_current_temp());
    rtdb_set_current_temp(-5);
    TEST_ASSERT_EQUAL_INT16(-5, rtdb_get_current_temp());
}

/* 7) Testa mudança independente de max/min */
void test_independent_min_max(void) {
    rtdb_set_max_temp(100);
    rtdb_set_min_temp(20);
    TEST_ASSERT_EQUAL_INT16(100, rtdb_get_max_temp());
    TEST_ASSERT_EQUAL_INT16(20,  rtdb_get_min_temp());
}

/* 8) Testa heater getter/setter */
void test_heater_get_set(void) {
    TEST_ASSERT_FALSE(rtdb_get_heater());
    rtdb_set_heater(true);
    TEST_ASSERT_TRUE(rtdb_get_heater());
    rtdb_set_heater(false);
    TEST_ASSERT_FALSE(rtdb_get_heater());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_system_off);
    RUN_TEST(test_toggle_system_on_off);
    RUN_TEST(test_default_setpoint_and_limits);
    RUN_TEST(test_setpoint_respects_max);
    RUN_TEST(test_setpoint_respects_min);
    RUN_TEST(test_current_temp_set_get);
    RUN_TEST(test_independent_min_max);
    RUN_TEST(test_heater_get_set);
    return UNITY_END();
}
