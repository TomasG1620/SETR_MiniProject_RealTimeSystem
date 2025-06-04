#include "unity.h"
#include "uartcomm_dummy.h"
#include "rtdb_dummy.h"
#include <string.h>

/* Prototype para acessar o buffer de saída */
extern const char *get_uart_test_output(void);
extern void clear_uart_test_output(void);

void setUp(void) {
    rtdb_dummy_init();
    clear_uart_test_output();
}

void tearDown(void) {
 
}

/* 1) Testa calculate_checksum() */
void test_checksum_values(void) {
    uint8_t d1[] = { 'A' };      
    TEST_ASSERT_EQUAL_UINT8(65, calculate_checksum(d1, 1));

    uint8_t d2[] = { 'A','B','C' };
    TEST_ASSERT_EQUAL_UINT8(198, calculate_checksum(d2, 3));

    uint8_t d3[] = { 'M','0','2','5' };
    TEST_ASSERT_EQUAL_UINT8(228, calculate_checksum(d3, 4));
}

/* 2) Framing erro: len < 6 */
void test_handle_frame_too_short(void) {
    uint8_t buf[] = { '#','C','0','!' }; 
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ef171!", get_uart_test_output());
}

/* 3) Framing erro: falta ‘#’ no início */
void test_handle_missing_hash(void) {
    uint8_t buf[] = { 'X','C','0','0','0','!' };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ef171!", get_uart_test_output());
}

/* 4) Framing erro: falta ‘!’ no final */
void test_handle_missing_exclamation(void) {
    uint8_t buf[] = { '#','C','0','0','0',' ' }; 
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ef171!", get_uart_test_output());
}

/* 5) Comando inválido + checksum correto → Ei */
void test_invalid_command_checksum_ok(void) {
    uint8_t buf[] = { '#','X',
                      '0','0','0',
                      '2','3','2',
                      '!' 
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 6) Comando inválido + checksum incorreto → Es + Ei */
void test_invalid_command_checksum_bad(void) {
    uint8_t buf[] = { '#','X',
                      '0','0','0',
                      '0','0','0',
                      '!'
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Es184!#Ei174!", get_uart_test_output());
}

/* 7) Comando “C”: consulta current_temp */
void test_query_current_temp(void) {
    rtdb_dummy_set_current_temp(42);
    uint8_t buf[] = {
        '#','C',
        '2','1','7',
        '!'
    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#c042249!", get_uart_test_output());
}

/* 8) Comando “M”: set max_temp, DATA_len!=3 → Ei */
void test_set_max_temp_bad_length(void) {
    uint8_t buf[] = { '#','M',
                      '1','2',      // só 2 bytes em vez de 3
                      '0','0','0',
                      '!'
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 9) Comando “M”: set max_temp < min_temp → Ei */

void test_set_max_temp_below_min(void) {
    rtdb_dummy_set_min_temp(10);
    char frame[16];
    snprintf(frame, sizeof(frame), "#M005226!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 10) Comando “M”: set max_temp válido */
void test_set_max_temp_ok(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#M030224!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_INT16(30, rtdb_dummy_get_max_temp());
    TEST_ASSERT_EQUAL_STRING("#Eo180!", get_uart_test_output());
}

/* 11) Comando “m”: set min_temp, DATA_len!=3 → Ei */
void test_set_min_temp_bad_length(void) {
    uint8_t buf[] = { '#','m',
                      '0','5',      
                      '0','0','0',
                      '!'
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 12) Comando “m”: set min_temp > max_temp → Ei */
void test_set_min_temp_above_max(void) {
    rtdb_dummy_set_max_temp(20);
    char frame[16];
    snprintf(frame, sizeof(frame), "#m030000!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 13) Comando “m”: set min_temp válido */
void test_set_min_temp_ok(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#m025004!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_INT16(25, rtdb_dummy_get_min_temp());
    TEST_ASSERT_EQUAL_STRING("#Eo180!", get_uart_test_output());
}

/* 14) Comando “R”: set sampling_rate, DATA_len!=4 → Ei */
void test_set_sampling_rate_bad_length(void) {
    uint8_t buf[] = { '#','R',
                      '1','2','3',  
                      '0','0','0',
                      '!'
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 15) Comando “R”: set sampling_rate < 10 → Ei */
void test_set_sampling_rate_below_min_uart(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#R0005231!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 16) Comando “R”: set sampling_rate válido */
void test_set_sampling_rate_ok_uart(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#R1000019!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_UINT32(1000, rtdb_dummy_get_sampling_rate());
    TEST_ASSERT_EQUAL_STRING("#Eo180!", get_uart_test_output());
}

/* 17) Comando “r”: get sampling_rate, data_len!=0 → Ei */
void test_get_sampling_rate_bad_length(void) {
    uint8_t buf[] = { '#','r',
                      '0','1','2','3', 
                      '0','0','0',
                      '!'
                    };
    handle_command(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

/* 18) Comando “r”: get sampling_rate, data_len=0, mas checksum errado → Es */
void test_get_sampling_rate_checksum_bad(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#r000!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#Es184!", get_uart_test_output());
}

/* 19) Comando “r”: get sampling_rate, data_len=0, checksum correto → sxxx (4 dígitos) */
void test_get_sampling_rate_ok(void) {
    rtdb_dummy_set_sampling_rate(3456);
    char frame[16];
    snprintf(frame, sizeof(frame), "#r114!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#s3456069!", get_uart_test_output());
}

/* 20) Comando “E”: toggle system_on (ligar) com payload '0' */
void test_system_on_via_uart(void) {
    rtdb_dummy_set_system_on(false);
    char frame[16];
    snprintf(frame, sizeof(frame), "#E0117!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_TRUE(rtdb_dummy_get_system_on());
    TEST_ASSERT_EQUAL_STRING("#Eo180!", get_uart_test_output());
}

/* 21) Comando “E”: toggle system_on (desligar) com payload '1' */
void test_system_off_via_uart(void) {
    rtdb_dummy_set_system_on(true);
    char frame[16];
    snprintf(frame, sizeof(frame), "#E1118!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_FALSE(rtdb_dummy_get_system_on());
    TEST_ASSERT_EQUAL_STRING("#Eo180!", get_uart_test_output());
}

/* 22) Comando “E”: payload inválido (por ex. '2') → Ei */
void test_system_toggle_invalid_payload(void) {
    char frame[16];
    snprintf(frame, sizeof(frame), "#E2119!");
    handle_command((const uint8_t *)frame, strlen(frame));
    TEST_ASSERT_EQUAL_STRING("#Ei174!", get_uart_test_output());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_checksum_values);
    RUN_TEST(test_handle_frame_too_short);
    RUN_TEST(test_handle_missing_hash);
    RUN_TEST(test_handle_missing_exclamation);
    RUN_TEST(test_invalid_command_checksum_ok);
    RUN_TEST(test_invalid_command_checksum_bad);
    RUN_TEST(test_query_current_temp);
    RUN_TEST(test_set_max_temp_bad_length);
    RUN_TEST(test_set_max_temp_below_min);
    RUN_TEST(test_set_max_temp_ok);
    RUN_TEST(test_set_min_temp_bad_length);
    RUN_TEST(test_set_min_temp_above_max);
    RUN_TEST(test_set_min_temp_ok);
    RUN_TEST(test_set_sampling_rate_bad_length);
    RUN_TEST(test_set_sampling_rate_below_min_uart);
    RUN_TEST(test_set_sampling_rate_ok_uart);
    RUN_TEST(test_get_sampling_rate_bad_length);
    RUN_TEST(test_get_sampling_rate_checksum_bad);
    RUN_TEST(test_get_sampling_rate_ok);
    RUN_TEST(test_system_on_via_uart);
    RUN_TEST(test_system_off_via_uart);
    RUN_TEST(test_system_toggle_invalid_payload);
    return UNITY_END();
}

