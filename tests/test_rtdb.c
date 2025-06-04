#include "unity.h"
#include "rtdb_dummy.h"
#include <stdint.h>

/* 1) Zera o RTDB para valores default */
void setUp(void) {
    rtdb_dummy_init();
}

void tearDown(void) {
    
}

/* 1) Testa estado inicial (system_on true) */
void test_initial_system_on(void) {
    TEST_ASSERT_TRUE(rtdb_dummy_get_system_on());
}

/* 2) Testa toggling system_on/off */
void test_toggle_system_on_off(void) {
    rtdb_dummy_set_system_on(false);
    TEST_ASSERT_FALSE(rtdb_dummy_get_system_on());

    rtdb_dummy_set_system_on(true);
    TEST_ASSERT_TRUE(rtdb_dummy_get_system_on());
}

/* 3) Testa default setpoint e limites de min/max */
void test_default_setpoint_and_limits(void) {
    TEST_ASSERT_EQUAL_INT16(26, rtdb_dummy_get_setpoint());
    TEST_ASSERT_EQUAL_INT16(80, rtdb_dummy_get_max_temp());
    TEST_ASSERT_EQUAL_INT16(20, rtdb_dummy_get_min_temp());
}

/* 4) Testa set_max_temp: se setpoint > max → setpoint fica em max */
void test_setpoint_respects_max(void) {
    rtdb_dummy_set_max_temp(30);
    rtdb_dummy_set_setpoint(40);
    TEST_ASSERT_EQUAL_INT16(30, rtdb_dummy_get_setpoint());
    
    rtdb_dummy_set_setpoint(30);
    TEST_ASSERT_EQUAL_INT16(30, rtdb_dummy_get_setpoint());
}

/* 5) Testa set_min_temp: se setpoint < min → setpoint fica em min */
void test_setpoint_respects_min(void) {
    rtdb_dummy_set_min_temp(10);
    rtdb_dummy_set_setpoint(5);
    TEST_ASSERT_EQUAL_INT16(10, rtdb_dummy_get_setpoint());
    
    rtdb_dummy_set_setpoint(10);
    TEST_ASSERT_EQUAL_INT16(10, rtdb_dummy_get_setpoint());
}

/* 6) Testa current_temp getter/setter */
void test_current_temp_set_get(void) {
    rtdb_dummy_set_current_temp(22);
    TEST_ASSERT_EQUAL_INT16(22, rtdb_dummy_get_current_temp());
    
    rtdb_dummy_set_current_temp(-5);
    TEST_ASSERT_EQUAL_INT16(-5, rtdb_dummy_get_current_temp());
}

/* 7) Testa mudança de max/min */
void test_independent_min_max(void) {
    rtdb_dummy_set_max_temp(100);
    rtdb_dummy_set_min_temp(20);
    TEST_ASSERT_EQUAL_INT16(100, rtdb_dummy_get_max_temp());
    TEST_ASSERT_EQUAL_INT16(20,  rtdb_dummy_get_min_temp());
}

/* 8) Testa heater getter/setter */
void test_heater_get_set(void) {
    TEST_ASSERT_FALSE(rtdb_dummy_get_heater());
    rtdb_dummy_set_heater(true);
    
    TEST_ASSERT_TRUE(rtdb_dummy_get_heater());
    rtdb_dummy_set_heater(false);
    
    TEST_ASSERT_FALSE(rtdb_dummy_get_heater());
}

/* 9) Testa default sampling_rate */
void test_default_sampling_rate(void) {
    TEST_ASSERT_EQUAL_UINT32(1000, rtdb_dummy_get_sampling_rate());
}

/* 10) Testa set_sampling_rate: se < 10 → 10 */
void test_set_sampling_rate_below_min(void) {
    rtdb_dummy_set_sampling_rate(5);
    TEST_ASSERT_EQUAL_UINT32(10, rtdb_dummy_get_sampling_rate());
}

/* 11) Testa set_sampling_rate: se > 60000 → 60000 */
void test_set_sampling_rate_above_max(void) {
    rtdb_dummy_set_sampling_rate(70000);
    TEST_ASSERT_EQUAL_UINT32(60000, rtdb_dummy_get_sampling_rate());
}

/* 12) Testa set_sampling_rate valores válidos (entre 10 e 60000) */
void test_set_sampling_rate_valid(void) {
    rtdb_dummy_set_sampling_rate(500);
    TEST_ASSERT_EQUAL_UINT32(500, rtdb_dummy_get_sampling_rate());

    rtdb_dummy_set_sampling_rate(60000);
    TEST_ASSERT_EQUAL_UINT32(60000, rtdb_dummy_get_sampling_rate());

    rtdb_dummy_set_sampling_rate(10);
    TEST_ASSERT_EQUAL_UINT32(10, rtdb_dummy_get_sampling_rate());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_system_on);
    RUN_TEST(test_toggle_system_on_off);
    RUN_TEST(test_default_setpoint_and_limits);
    RUN_TEST(test_setpoint_respects_max);
    RUN_TEST(test_setpoint_respects_min);
    RUN_TEST(test_current_temp_set_get);
    RUN_TEST(test_independent_min_max);
    RUN_TEST(test_heater_get_set);
    RUN_TEST(test_default_sampling_rate);
    RUN_TEST(test_set_sampling_rate_below_min);
    RUN_TEST(test_set_sampling_rate_above_max);
    RUN_TEST(test_set_sampling_rate_valid);
    return UNITY_END();
}

