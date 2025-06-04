#include "uartcomm_dummy.h"
#include "rtdb_dummy.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* --------------------------------------------------------------------------
 * Buffer estático para capturar tudo que seria enviado pela UART
 * -------------------------------------------------------------------------- */

static char uart_test_buffer[2048];
static size_t uart_test_index;

/* Limpa o buffer */
void clear_uart_test_output(void)
{
    uart_test_index = 0;
    uart_test_buffer[0] = '\0';
}

/* Retorna a string acumulada */
const char *get_uart_test_output(void)
{
    return uart_test_buffer;
}

/* Envia um byte “virtual” para a UART (acumula no buffer) */
static void send_byte(char b)
{
    if (uart_test_index < sizeof(uart_test_buffer) - 1) {
        uart_test_buffer[uart_test_index++] = b;
        uart_test_buffer[uart_test_index] = '\0';
    }
}

/* Converte um valor 0..255 em três dígitos ASCII (‘000’..’255’) */
static void byte_to_3ascii(uint8_t byte, char out[4])
{
    out[0] = '0' + (byte / 100) % 10;
    out[1] = '0' + (byte / 10) % 10;
    out[2] = '0' + (byte % 10);
    out[3] = '\0';
}

/* Envia um frame completo: # + cmd + data(ASCII) + CS(3 dígitos) + ! */
static void send_frame(char cmd, const char *data, size_t data_len)
{
    /* Montamos o frame em memória */
    uint8_t frame[1 + 1 + 64 + 3 + 1];
    size_t pos = 0;

    /* Cabeçalho */
    frame[pos++] = (uint8_t)'#';
    frame[pos++] = (uint8_t)cmd;

    /* DATA, se houver */
    for (size_t i = 0; i < data_len; i++) {
        frame[pos++] = (uint8_t)data[i];
    }

    /* Calcula checksum */
    uint8_t sum = (uint8_t)cmd;
    for (size_t i = 0; i < data_len; i++) {
        sum += (uint8_t)data[i];
    }

    /* Converte checksum em 3 ASCII */
    char cs_ascii[4];
    byte_to_3ascii(sum, cs_ascii);
    for (int i = 0; i < 3; i++) {
        frame[pos++] = (uint8_t)cs_ascii[i];
    }

    /* Fim do frame */
    frame[pos++] = (uint8_t)'!';

    /* “Envia” (acumula no buffer) */
    for (size_t i = 0; i < pos; i++) {
        send_byte((char)frame[i]);
    }
}

/* Envia um ACK de erro/OK */
static void send_ack(char code)
{
    char data[2] = { code, '\0' };
    send_frame('E', data, 1);
}

/* Cálculo de checksum módulo-256 */
uint8_t calculate_checksum(const uint8_t *buf, size_t len)
{
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return (uint8_t)(sum & 0xFF);
}

/* --------------------------------------------------------------------------
 * handle_command “dummy”
 *
 *  Lógica:
 *   1) Se len < 6 → framing error (#Ef<CS>!).
 *   2) Se buf[0] != '#' ou buf[len-1] != '!' → framing error.
 *   3) Extrai cmd = buf[1].
 *   4) Extrai cs_rcv a partir dos três caracteres antes do '!' (buf[len-4..len-2]).
 *   5) Se cmd == 'C': 
 *        • Lê temperatura: cur = rtdb_dummy_get_current_temp().
 *        • Monta cur_str (3 dígitos).
 *        • Calcula sum_expected = 'C' + cur_str[0]+ cur_str[1]+ cur_str[2].
 *        • Se cs_rcv != sum_expected → send_ack('s'); return.
 *        • Caso contrário: send_frame('c', cur_str, 3); return.
 *   6) Calcule data_len = len - 6; data_ptr = buf + 2.
 *   7) Se cmd == 'M': (similar ao código original)
 *        • Se data_len != 3 → send_ack('i'); return.
 *        • sum_full = 'M' + data_ptr[0..2]; se sum_full != cs_rcv → send_ack('s'); return.
 *        • val = atoi(data_ptr); se val < rtdb_dummy_get_min_temp() → send_ack('i'); 
 *          else → rtdb_dummy_set_max_temp(val); send_ack('o').
 *        • return.
 *   8) Se cmd == 'm': (similar ao código original)
 *        • Se data_len != 3 → send_ack('i'); return.
 *        • sum_full = 'm' + data_ptr[0..2]; se sum_full != cs_rcv → send_ack('s'); return.
 *        • val = atoi(data_ptr); se val > rtdb_dummy_get_max_temp() → send_ack('i');
 *          else → rtdb_dummy_set_min_temp(val); send_ack('o').
 *        • return.
 *   9) Se cmd == 'R': (MUDANÇA AQUI — range antes do checksum)
 *        • Se data_len != 4 → send_ack('i'); return.
 *        • Extrai tmp = string de 4 dígitos. val = atoi(tmp).
 *        • Se val < 10 ou val > 9999 → send_ack('i'); return.
 *        • Agora calcula sum_full = 'R' + data_ptr[0..3]; se sum_full != cs_rcv → send_ack('s'); return.
 *        • Caso OK → rtdb_dummy_set_sampling_rate(val); send_ack('o'); return.
 *  10) Se cmd == 'r': (get sampling_rate, sem payload)
 *        • Se data_len != 0 → send_ack('i'); return.
 *        • sum_expected = 'r' (114); se sum_expected != cs_rcv → send_ack('s'); return.
 *        • sr = rtdb_dummy_get_sampling_rate(); clamp em 9999 se necessário.
 *        • Monta sr_str (4 dígitos) e send_frame('s', sr_str, 4). return.
 *  11) Se cmd == 'E': (toggle system_on)
 *        • Se data_len != 1 → send_ack('i'); return.
 *        • sum_full = 'E' + data_ptr[0]; se sum_full != cs_rcv → send_ack('s'); return.
 *        • c = data_ptr[0]; se c=='0' → ligar + send_ack('o');
 *                            se c=='1' → desligar + send_ack('o');
 *                            senão → send_ack('i').
 *        • return.
 *  12) Se cmd == 'S': (set controller params)
 *        • Se data_len < 1 → send_ack('i'); return.
 *        • sum_full = 'S' + data_ptr[0..(data_len-1)]; se sum_full != cs_rcv → send_ack('s'); return.
 *        • Caso OK → send_ack('o'); return.
 *  13) Qualquer outro: 
 *        • sum_full = cmd + data_ptr[0..(data_len-1)]; 
 *        • Se sum_full != cs_rcv → send_ack('s'); send_ack('i');
 *          senão → send_ack('i').
 * -------------------------------------------------------------------------- */

void handle_command(const uint8_t *buf, size_t len)
{
    /* Tamanho mínimo = 6 */
    if (len < 6) {
        send_ack('f');
        return;
    }

    /* Procura por '#...!' */
    if (buf[0] != (uint8_t)'#' || buf[len - 1] != (uint8_t)'!') {
        send_ack('f');
        return;
    }

    /* Extrai CMD */
    char cmd = (char)buf[1];

    /* Extrai CS recebido */
    char cs_str[4] = {
        (char)buf[len - 4],
        (char)buf[len - 3],
        (char)buf[len - 2],
        '\0'
    };
    uint8_t cs_rcv = (uint8_t)atoi(cs_str);

    /* Caso “C” (consulta temperatura) */
    if (cmd == 'C') {
        int cur = rtdb_dummy_get_current_temp();
        if (cur < 0) cur = 0;
        else if (cur > 999) cur = 999;

        char cur_str[4];
        cur_str[0] = '0' + (cur / 100) % 10;
        cur_str[1] = '0' + ((cur / 10) % 10);
        cur_str[2] = '0' + (cur % 10);
        cur_str[3] = '\0';

        uint8_t sum_expected = (uint8_t)'C'
                              + (uint8_t)cur_str[0]
                              + (uint8_t)cur_str[1]
                              + (uint8_t)cur_str[2];
        if (sum_expected != cs_rcv) {
            send_ack('s');
            return;
        }

        send_frame('c', cur_str, 3);
        return;
    }

    /* Calcula data_len */
    size_t data_len = len - 6;       
    const uint8_t *data_ptr = buf + 2;

    /* “M” define max_temp */
    if (cmd == 'M') {
        if (data_len != 3) {
            send_ack('i');
            return;
        }
        uint8_t sum_full = (uint8_t)'M';
        for (size_t i = 0; i < 3; i++) {
            sum_full += data_ptr[i];
        }
        if (sum_full != cs_rcv) {
            send_ack('s');
            return;
        }
        char tmp[4] = {
            (char)data_ptr[0],
            (char)data_ptr[1],
            (char)data_ptr[2],
            '\0'
        };
        int val = atoi(tmp);
        if (val < rtdb_dummy_get_min_temp()) {
            send_ack('i');
        } else {
            rtdb_dummy_set_max_temp((int16_t)val);
            send_ack('o');
        }
        return;
    }

    /* “m” define min_temp */
    if (cmd == 'm') {
        if (data_len != 3) {
            send_ack('i');
            return;
        }
        uint8_t sum_full = (uint8_t)'m';
        for (size_t i = 0; i < 3; i++) {
            sum_full += data_ptr[i];
        }
        if (sum_full != cs_rcv) {
            send_ack('s');
            return;
        }
        char tmp[4] = {
            (char)data_ptr[0],
            (char)data_ptr[1],
            (char)data_ptr[2],
            '\0'
        };
        int val = atoi(tmp);
        if (val > rtdb_dummy_get_max_temp()) {
            send_ack('i');
        } else {
            rtdb_dummy_set_min_temp((int16_t)val);
            send_ack('o');
        }
        return;
    }

    /* “R” define sampling_rate (4 dígitos) */
    if (cmd == 'R') {
        if (data_len != 4) {
            send_ack('i');
            return;
        }
        /* Extrai valor antes de qualquer checksum */
        char tmp[5] = {
            (char)data_ptr[0],
            (char)data_ptr[1],
            (char)data_ptr[2],
            (char)data_ptr[3],
            '\0'
        };
        int val = atoi(tmp);
        /* Se estiver fora do intervalo, invalid sem checar checksum */
        if (val < 10 || val > 9999) {
            send_ack('i');
            return;
        }
        /* Agora que o valor está no intervalo, verifica checksum */
        uint8_t sum_full = (uint8_t)'R';
        for (size_t i = 0; i < 4; i++) {
            sum_full += data_ptr[i];
        }
        if (sum_full != cs_rcv) {
            send_ack('s');
            return;
        }
        rtdb_dummy_set_sampling_rate((uint32_t)val);
        send_ack('o');
        return;
    }

    /* “r” consulta sampling_rate (sem payload) */
    if (cmd == 'r') {
        if (data_len != 0) {
            send_ack('i');
            return;
        }
        uint8_t sum_expected = (uint8_t)'r'; /* 114 */
        if (sum_expected != cs_rcv) {
            send_ack('s');
            return;
        }
        uint32_t sr = rtdb_dummy_get_sampling_rate();
        if (sr > 9999U) sr = 9999U;
        char out[5];
        out[0] = '0' + ((sr / 1000) % 10);
        out[1] = '0' + ((sr / 100)  % 10);
        out[2] = '0' + ((sr / 10)   % 10);
        out[3] = '0' + (sr % 10);
        out[4] = '\0';
        send_frame('s', out, 4);
        return;
    }

    /* “E” liga/desliga sistema (payload '0' ou '1') */
    if (cmd == 'E') {
        if (data_len != 1) {
            send_ack('i');
            return;
        }
        uint8_t sum_full = (uint8_t)'E' + (uint8_t)data_ptr[0];
        if (sum_full != cs_rcv) {
            send_ack('s');
            return;
        }
        char c = (char)data_ptr[0];
        if (c == '0') {
            rtdb_dummy_set_system_on(true);
            send_ack('o');
        } else if (c == '1') {
            rtdb_dummy_set_system_on(false);
            send_ack('o');
        } else {
            send_ack('i');
        }
        return;
    }

    /* “S” set controller params (qualquer data_len ≥ 1) */
    if (cmd == 'S') {
        if (data_len < 1) {
            send_ack('i');
            return;
        }
        uint8_t sum_full = (uint8_t)'S';
        for (size_t i = 0; i < data_len; i++) {
            sum_full += data_ptr[i];
        }
        if (sum_full != cs_rcv) {
            send_ack('s');
            return;
        }
        send_ack('o');
        return;
    }

    /* 13) Qualquer outro comando: invalid + possível checksum error */
    {
        uint8_t sum_full = (uint8_t)cmd;
        for (size_t i = 0; i < data_len; i++) {
            sum_full += data_ptr[i];
        }
        if (sum_full != cs_rcv) {
            send_ack('s');
            send_ack('i');
        } else {
            send_ack('i');
        }
    }
}

