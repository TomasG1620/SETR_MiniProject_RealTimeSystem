// tests/test_uartcomm.c

#include "unity.h"
#include "uartcomm.h"
#include "rtdb.h"
#include <string.h>

/* Prototype para acessar o buffer de saída simulado em UNIT_TEST */
#ifdef UNIT_TEST
extern const char *get_uart_test_output(void);
extern void clear_uart_test_output(void);
#endif

void setUp(void) {
    rtdb_init_default();
#ifdef UNIT_TEST
    clear_uart_test_output();
#endif
}

void tearDown(void) {
    /* Nada */
}

/* 1) Testa calculate_checksum() */
void test_checksum_values(void) {
    uint8_t d1[] = { 'A' };       // 65 → 65
    TEST_ASSERT_EQUAL_UINT8(65, calculate_checksum(d1, 1));

    uint8_t d2[] = { 'A','B','C' };// 65+66+67=198
    TEST_ASSERT_EQUAL_UINT8(198, calculate_checksum(d2, 3));

    uint8_t d3[] = { 'M','0','2','5' };// 77+48+50+53=228
    TEST_ASSERT_EQUAL_UINT8(228, calculate_checksum(d3, 4));
}

/* 2) Framing error: len < 6 */
void test_handle_frame_too_short(void) {
    uint8_t buf[] = { '#','C','0','!' }; // len=4 < 6
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    // Espera exatamente “#Ef000!” – “f” (framing), CS=“000”
    TEST_ASSERT_EQUAL_STRING("#Ef000!", get_uart_test_output());
#endif
}

/* 3) Framing error: falta ‘#’ no início */
void test_handle_missing_hash(void) {
    uint8_t buf[] = { 'X','C','0','0','0','!' }; // começa com 'X'
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ef000!", get_uart_test_output());
#endif
}

/* 4) Framing error: falta ‘!’ no final */
void test_handle_missing_exclamation(void) {
    uint8_t buf[] = { '#','C','0','0','0',' ' }; // termina com espaço
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ef000!", get_uart_test_output());
#endif
}

/* 5) Invalid command + checksum correto → Ei */
void test_invalid_command_checksum_ok(void) {
    // Construir frame: "#X000232!" onde “232” é checksum correto de {'X','0','0','0'}
    // 'X'=88 + '0'=48 + '0'=48 + '0'=48 = 232 → “232”
    uint8_t buf[] = { '#','X',
                      '0','0','0',
                      '2','3','2',
                      '!' 
                    };
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    // checksum ok mas cmd='X' inválido → apenas “Ei000!”
    TEST_ASSERT_EQUAL_STRING("#Ei000!", get_uart_test_output());
#endif
}

/* 6) Invalid command + checksum incorreto → Es + Ei */
void test_invalid_command_checksum_bad(void) {
    // Mesmo comando ‘X000’, mas colocamos CS="000" (incorreto)
    uint8_t buf[] = { '#','X',
                      '0','0','0',
                      '0','0','0',
                      '!'
                    };
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    // checksum errado (“Es000!”) e depois “Ei000!”
    TEST_ASSERT_EQUAL_STRING("#Es000!#Ei000!", get_uart_test_output());
#endif
}

/* 7) Comando “C”: consulta current_temp */
void test_query_current_temp(void) {
    rtdb_set_current_temp(42);
    // Cur=42 → “042”; checksum = 'C'(67)+ '0'(48)+ '4'(52)+ '2'(50) = 217 → “217”
    uint8_t buf[] = {
        '#','C',
        '2','1','7',
        '!'
    };
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    // Resposta: “#c042249!” → ‘c’=99 + '0'=48 + '4'=52 + '2'=50 = 249 → “249”
    TEST_ASSERT_EQUAL_STRING("#c042249!", get_uart_test_output());
#endif
}

/* 8) Comando “M”: set max_temp, DATA_len!=3 → Ei */
void test_set_max_temp_bad_length(void) {
    uint8_t buf[] = { '#','M',
                      '1','2',      // só 2 bytes em vez de 3
                      '0','0','0',
                      '!'
                    };
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ei000!", get_uart_test_output());
#endif
}

/* 9) Comando “M”: set max_temp < min_temp → Ei
   Primeiro definimos min_temp alto, depois tentamos enviar um M005 (5 < 10)  */
void test_set_max_temp_below_min(void) {
    rtdb_set_min_temp(10);
    // M005: 'M'=77 + '0'=48 + '0'=48 + '5'=53 = 226 → “226”
    char frame[16];
    snprintf(frame, sizeof(frame), "#M005226!");
    handle_command(NULL, (uint8_t *)frame, strlen(frame));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ei000!", get_uart_test_output());
#endif
}

/* 10) Comando “M”: set max_temp válido */
void test_set_max_temp_ok(void) {
    // Dados originais: min_temp=0, max_temp=80
    // Vamos fazer M050 (50). checksum = 'M'(77)+'0'(48)+'5'(53)+'0'(48) = 226 → “226”
    char frame[16];
    snprintf(frame, sizeof(frame), "#M050226!");
    handle_command(NULL, (uint8_t *)frame, strlen(frame));
    // Verifica se RTDB foi atualizado
    TEST_ASSERT_EQUAL_INT16(50, rtdb_get_max_temp());
#ifdef UNIT_TEST
    // Resposta deve começar com “#Eo”
    TEST_ASSERT_TRUE(strncmp(get_uart_test_output(), "#Eo", 3) == 0);
#endif
}

/* 11) Comando “m”: set min_temp, DATA_len!=3 → Ei */
void test_set_min_temp_bad_length(void) {
    uint8_t buf[] = { '#','m',
                      '0','5',      // apenas 2 bytes
                      '0','0','0',
                      '!'
                    };
    handle_command(NULL, buf, sizeof(buf));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ei000!", get_uart_test_output());
#endif
}

/* 12) Comando “m”: set min_temp > max_temp → Ei */
void test_set_min_temp_above_max(void) {
    rtdb_set_max_temp(20);
    // m030: 'm'(109)+'0'(48)+'3'(51)+'0'(48) = 256 &0xFF = 0 → “000”
    char frame[16];
    snprintf(frame, sizeof(frame), "#m030000!");
    handle_command(NULL, (uint8_t *)frame, strlen(frame));
#ifdef UNIT_TEST
    TEST_ASSERT_EQUAL_STRING("#Ei000!", get_uart_test_output());
#endif
}

/* 13) Comando “m”: set min_temp válido */
void test_set_min_temp_ok(void) {
    // max_temp=80, tentamos m010 (10). checksum = 109+48+49+48 = 254 → “254”
    char frame[16];
    snprintf(frame, sizeof(frame), "#m010254!");
    handle_command(NULL, (uint8_t *)frame, strlen(frame));
    // Verifica RTDB
    TEST_ASSERT_EQUAL_INT16(10, rtdb_get_min_temp());
#ifdef UNIT_TEST
    TEST_ASSERT_TRUE(strncmp(get_uart_test_output(), "#Eo", 3) == 0);
#endif
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
    return UNITY_END();
}
