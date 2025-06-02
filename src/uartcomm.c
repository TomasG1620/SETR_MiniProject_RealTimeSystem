/**
 * @file uartcomm.c
 * @brief UART communication module: parser de comandos + framing
 */

#include "uartcomm.h"
#include "rtdb.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#define UART_STACK_SIZE 1024U
#define UART_PRIORITY   5U
#define UART_BUF_SIZE   64U

/* =========================================================================== 
 * Declarações internas 
 * =========================================================================== */
static uint8_t calculate_checksum(const uint8_t *buf, size_t len);
static void    send_bytes(const struct device *dev, const uint8_t *data, size_t len);
static void    send_frame(const struct device *dev, char cmd, const char *data, size_t data_len);
static void    send_ack(const struct device *dev, char code);
static void    handle_command(const struct device *dev, const uint8_t *buf, size_t len);
static void    uart_task(void *p1, void *p2, void *p3);

K_THREAD_STACK_DEFINE(uart_stack, UART_STACK_SIZE);
static struct k_thread uart_thread_data;

/**
 * @brief Inicializa e dispara a thread de UART
 */
void uart_comm_init(void)
{
    k_thread_create(&uart_thread_data, uart_stack, UART_STACK_SIZE,
                    uart_task, NULL, NULL, NULL,
                    UART_PRIORITY, 0, K_NO_WAIT);
}

/**
 * @brief Thread que faz polling, enquadra bytes e despacha handle_command()
 */
static void uart_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    if (!device_is_ready(uart_dev)) {
        printk("UART not ready\n");
        return;
    }

    uint8_t buf[UART_BUF_SIZE];
    size_t  idx = 0U;
    uint8_t byte;

    for (;;) {
        if (uart_poll_in(uart_dev, &byte) == 0) {

            // 1) '!' sem ter aberto frame em '#': framing error imediato
            if ((byte == '!') && (idx == 0U)) {
                send_ack(uart_dev, 'f');
                continue;
            }

            // 2) Novo '#' vindo enquanto idx>0 (frame não fechou com '!'): framing error no anterior
            if ((byte == '#') && (idx > 0U)) {
                send_ack(uart_dev, 'f');
                idx = 0U;
                buf[idx++] = '#';
                continue;
            }

            // 3) ENTER ('\r' ou '\n') no meio de um frame (idx>0): framing error
            if (((byte == '\n') || (byte == '\r')) && (idx > 0U)) {
                send_ack(uart_dev, 'f');
                idx = 0U;
                continue;
            }

            // 4) Se for '#', com idx==0, começa novo frame
            if (byte == '#') {
                idx = 0U;
                buf[idx++] = byte;
                continue;
            }

            // 5) Se estivermos dentro de um frame (idx>0), vamos copiando até achar '!' ou encher buffer
            if (idx > 0U) {
                buf[idx++] = byte;

                // 5a) Se for '!' → fim de frame completo
                if (byte == '!') {
                    handle_command(uart_dev, buf, idx);
                    idx = 0U;
                    continue;
                }

                // 5b) Se encheu buffer sem ver '!' → framing error
                if (idx >= UART_BUF_SIZE) {
                    send_ack(uart_dev, 'f');
                    idx = 0U;
                    continue;
                }

                // Senão, continua acumulando bytes do frame
                continue;
            }

            // 6) Qualquer outro byte fora de um frame (“idx==0” e não é nem '!' nem '#') → ignora
        }

        k_sleep(K_MSEC(10));
    }
}

/**
 * @brief Calcula checksum módulo-256 sobre buf[0..len-1]
 */
static uint8_t calculate_checksum(const uint8_t *buf, size_t len)
{
    uint16_t sum = 0U;
    for (size_t i = 0U; i < len; i++) {
        sum += buf[i];
    }
    return (uint8_t)(sum & 0xFFU);
}

/**
 * @brief Envia raw bytes pela UART
 */
static void send_bytes(const struct device *dev, const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        uart_poll_out(dev, data[i]);
    }
}

/**
 * @brief Constrói o frame: #CMD + DATA + CS(3 dígitos ASCII) + "!"
 *
 *   - cmd: caractere único (por ex. 'E', 'c', 'M', 'm', etc.)
 *   - data: ponteiro para bytes ASCII (por ex. "045")
 *   - data_len: comprimento de “data”
 *   - CS = soma(CMD + cada byte de DATA) & 0xFF, convertido em 3 dígitos ASCII
 */
static void send_frame(const struct device *dev, char cmd, const char *data, size_t data_len)
{
    // 1byte('#') + 1byte(cmd) + data_len + 3bytes(checksum) + 1byte('!')
    uint8_t frame[1U + 1U + UART_BUF_SIZE + 3U + 1U];
    size_t  pos = 0U;

    frame[pos++] = '#';
    frame[pos++] = (uint8_t)cmd;
    for (size_t i = 0U; i < data_len; i++) {
        frame[pos++] = (uint8_t)data[i];
    }
    // Calcula checksum sobre [CMD] + DATA[0..data_len-1]
    uint8_t cs = calculate_checksum(&frame[1], 1U + data_len);
    // Converte em três dígitos ASCII
    frame[pos++] = '0' + (uint8_t)((cs / 100U) % 10U);
    frame[pos++] = '0' + (uint8_t)((cs / 10U) % 10U);
    frame[pos++] = '0' + (uint8_t)(cs % 10U);

    frame[pos++] = '!';
    send_bytes(dev, frame, pos);
}

/**
 * @brief Envia um ACK simples: E<code>!
 *   - code: 'o' (OK), 'f' (framing error), 's' (checksum error), 'i' (invalid command)
 */
static void send_ack(const struct device *dev, char code)
{
    send_frame(dev, 'E', &code, 1U);
}

/**
 * @brief Trata o frame completo em buf[0..len-1], onde buf[0]=='#' e buf[len-1]=='!'
 *
 *  - CMD = buf[1]
 *  - DATA = buf[2..(2+data_len-1)]
 *  - CS_str = buf[len-4..len-2] (3 dígitos ASCII)
 *  - data_len = len - 6   (descarta '#' (1), CMD (1), CS(3), '!'(1))
 *
 *  Suportado:
 *   - 'M' (maiúsculo): define max_temp
 *   - 'm' (minúsculo): define min_temp
 *   - 'C': consulta current_temp
 *   - 'S': configura parâmetros do controlador (stub)
 *
 *  Em caso de erro, envia:
 *   - framing error → send_ack(dev, 'f')
 *   - invalid command → send_ack(dev, 'i')
 *   - checksum error → send_ack(dev, 's')
 *  Se houver >1 erro (ex.: invalid + checksum), envia ambos (na ordem 'i' depois 's').
 */
static void handle_command(const struct device *dev, const uint8_t *buf, size_t len)
{
    // 1) Tamanho mínimo = 6 bytes:  # + CMD + CS(3) + !
    if (len < 6U) {
        send_ack(dev, 'f');  // framing error
        return;
    }
    // 2) Verifica '#' no início e '!' no fim
    if ((buf[0] != '#') || (buf[len - 1] != '!')) {
        send_ack(dev, 'f');
        return;
    }
    // 3) Extrai CMD
    char cmd = (char)buf[1];

    // 4) Extrai checksum recebido (3 dígitos ASCII)
    char cs_str[4] = {
        (char)buf[len - 4],
        (char)buf[len - 3],
        (char)buf[len - 2],
        '\0'
    };
    uint8_t cs_rcv = (uint8_t)atoi(cs_str);

    // 5) Determina o tamanho de DATA
    size_t data_len = len - 6U;  // retira '#', CMD, CS(3), '!'
    const uint8_t *data_ptr = &buf[2];

    // 6) Verifica se o comando é reconhecido
    bool cmd_valido = (cmd == 'M') || (cmd == 'm') || (cmd == 'C') || (cmd == 'S');
    if (!cmd_valido) {
        // Se não for nem 'M' nem 'm' nem 'C' nem 'S', trata erro de comando
        // --- Verifica também checksum de CMD isolado ---
        uint8_t cs_cmd = (uint8_t)cmd;
        if (cs_cmd != cs_rcv) {
            send_ack(dev, 's');  // checksum error
            send_ack(dev, 'i');  // invalid command
        } else {
            send_ack(dev, 'i');  // invalid command
        }
        return;
    }

    // 7) ĺevanta checksum completo sobre [CMD + DATA]
    uint8_t cs_calc = calculate_checksum((const uint8_t *)&buf[1], 1U + data_len);
    if (cs_calc != cs_rcv) {
        send_ack(dev, 's');  // checksum error
        return;
    }

    // 8) Se chegou aqui, o comando é válido e a checksum bateu
    switch (cmd) {
        case 'M': {  // #MxxxYYY! → set max temperature
            if (data_len != 3U) {
                send_ack(dev, 'i');  // invalid se não forem exatamente 3 dígitos
            } else {
                char tmp[4] = {
                    (char)data_ptr[0],
                    (char)data_ptr[1],
                    (char)data_ptr[2],
                    '\0'
                };
                int val = atoi(tmp);
                if (val < rtdb_get_min_temp()) {
                    // Não permitir max < min
                    send_ack(dev, 'i');
                } else {
                    rtdb_set_max_temp((int16_t)val);
                    printk("[UART] max_temp atualizado para %d°C\n", val);
                    send_ack(dev, 'o');
                }
            }
            break;
        }
        case 'm': {  // #mxxxYYY! → set min temperature
            if (data_len != 3U) {
                send_ack(dev, 'i');
            } else {
                char tmp[4] = {
                    (char)data_ptr[0],
                    (char)data_ptr[1],
                    (char)data_ptr[2],
                    '\0'
                };
                int val = atoi(tmp);
                if (val > rtdb_get_max_temp()) {
                    // Não permitir min > max
                    send_ack(dev, 'i');
                } else {
                    rtdb_set_min_temp((int16_t)val);
                    printk("[UART] min_temp atualizado para %d°C\n", val);
                    send_ack(dev, 'o');
                }
            }
            break;
        }
        case 'C': {  // #CYYY! → query current temperature
            int cur = rtdb_get_current_temp();
            if (cur < 0) {
                cur = 0;
            } else if (cur > 999) {
                cur = 999;
            }
            char out[4] = {
                (char)('0' + (cur / 100)),
                (char)('0' + ((cur / 10) % 10)),
                (char)('0' + (cur % 10)),
                '\0'
            };
            send_frame(dev, 'c', out, 3U);
            break;
        }
        case 'S': {  // #Sxxx...xxxYYY! → set controller parameters
            if (data_len < 1U) {
                send_ack(dev, 'i');
            } else {
                printk("[UART] parâmetros do controlador atualizados (DATA_len=%u)\n",
                       (unsigned)data_len);
                send_ack(dev, 'o');
            }
            break;
        }
        default:
            // Nunca deve chegar aqui
            send_ack(dev, 'i');
            break;
    }
}
