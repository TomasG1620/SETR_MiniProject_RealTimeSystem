// tests/test_controller.c
#include "unity.h"
#include "rtdb.h"
#include "controller.h"

/* Limpa o banco antes de cada teste */
void setUp(void) {
    rtdb_init_default();
    rtdb_set_heater(false);
}

void tearDown(void) {
    /* Sem limpeza extra */
}

/* 1) Se system_on=false → heater OFF */
void test_controller_system_off_always_off(void) {
    bool r = controller_compute(false, 25, 10, 0, 80, true);
    TEST_ASSERT_FALSE(r);
    r = controller_compute(false, 25, -5, 0, 80, false);
    TEST_ASSERT_FALSE(r);
}

/* 2) Se cur > max_temp → heater OFF (override) */
void test_controller_override_off_above_max(void) {
    bool r;
    rtdb_set_system_on(true);
    rtdb_set_max_temp(30);
    r = controller_compute(true, 25, 31, 0, 30, true);
    TEST_ASSERT_FALSE(r);
    r = controller_compute(true, 25, 50, 0, 30, false);
    TEST_ASSERT_FALSE(r);
}

/* 3) Se cur < min_temp → heater ON (override) */
void test_controller_override_on_below_min(void) {
    bool r;
    rtdb_set_system_on(true);
    rtdb_set_min_temp(20);
    r = controller_compute(true, 25, 19, 20, 80, false);
    TEST_ASSERT_TRUE(r);
    r = controller_compute(true, 25, -5, 20, 80, false);
    TEST_ASSERT_TRUE(r);
}

/* 4) Normal: se cur < setpoint e entre min/max → ON */
void test_controller_turn_on_below_sp(void) {
    bool r;
    rtdb_set_system_on(true);
    rtdb_set_min_temp(0);
    rtdb_set_max_temp(80);
    r = controller_compute(true, 25, 24, 0, 80, false);
    TEST_ASSERT_TRUE(r);
    r = controller_compute(true, 30, 29, 0, 80, true);
    TEST_ASSERT_TRUE(r);
}

/* 5) Normal: se cur >= setpoint e entre min/max → OFF */
void test_controller_turn_off_above_or_equal_sp(void) {
    bool r;
    rtdb_set_system_on(true);
    rtdb_set_min_temp(0);
    rtdb_set_max_temp(80);
    r = controller_compute(true, 25, 25, 0, 80, true);
    TEST_ASSERT_FALSE(r);
    r = controller_compute(true, 25, 30, 0, 80, true);
    TEST_ASSERT_FALSE(r);
}

/* 6) Sequência de casos */
void test_controller_sequence(void) {
    bool r;
    rtdb_set_system_on(true);
    rtdb_set_min_temp(5);
    rtdb_set_max_temp(50);

    r = controller_compute(true, 25, 10, 5, 50, false);
    TEST_ASSERT_TRUE(r);    // 10 < 25  (e >= min)
    r = controller_compute(true, 25, 25, 5, 50, true);
    TEST_ASSERT_FALSE(r);   // 25 >= 25
    r = controller_compute(true, 25, 26, 5, 50, false);
    TEST_ASSERT_FALSE(r);   // 26 > 25
    r = controller_compute(true, 25, 3, 5, 50, false);
    TEST_ASSERT_TRUE(r);    // 3 < 5   (below min)
    r = controller_compute(true, 25, 60, 5, 50, true);
    TEST_ASSERT_FALSE(r);   // 60 > 50 (above max)
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_controller_system_off_always_off);
    RUN_TEST(test_controller_override_off_above_max);
    RUN_TEST(test_controller_override_on_below_min);
    RUN_TEST(test_controller_turn_on_below_sp);
    RUN_TEST(test_controller_turn_off_above_or_equal_sp);
    RUN_TEST(test_controller_sequence);
    return UNITY_END();
}
