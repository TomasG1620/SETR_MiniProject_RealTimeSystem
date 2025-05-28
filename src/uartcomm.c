/**
 * @file uartcomm.c
 * @brief UART communication module: command parser and framing
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

/* Forward declarations */
static uint8_t calculate_checksum(const uint8_t *buf, size_t len);
static void send_bytes(const struct device *dev, const uint8_t *data, size_t len);
static void send_frame(const struct device *dev, char cmd, const char *data, size_t data_len);
static void send_ack(const struct device *dev, char code);
static void handle_command(const struct device *dev, const uint8_t *buf, size_t len);
static void uart_task(void *p1, void *p2, void *p3);

K_THREAD_STACK_DEFINE(uart_stack, UART_STACK_SIZE);
static struct k_thread uart_thread_data;

/**
 * @brief Initialize and start UART command task
 */
void uart_comm_init(void)
{
    k_thread_create(&uart_thread_data, uart_stack, UART_STACK_SIZE,
                    uart_task, NULL, NULL, NULL,
                    UART_PRIORITY, 0, K_NO_WAIT);
}

/**
 * @brief UART task: receives bytes, frames messages, and dispatches commands
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
    size_t idx = 0U;
    uint8_t byte;

    for (;;) {
        if (uart_poll_in(uart_dev, &byte) == 0) {
            if (byte == '#') {
                idx = 0U;
                buf[idx++] = byte;
            } else if (idx > 0U) {
                buf[idx++] = byte;
                if (byte == '!' || idx >= UART_BUF_SIZE) {
                    handle_command(uart_dev, buf, idx);
                    idx = 0U;
                }
            }
        }
        k_sleep(K_MSEC(10));
    }
}

/**
 * @brief Calculate modulo-256 checksum over buffer
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
 * @brief Send bytes over UART
 */
static void send_bytes(const struct device *dev, const uint8_t *data, size_t len)
{
    for (size_t i = 0U; i < len; i++) {
        uart_poll_out(dev, data[i]);
    }
}

/**
 * @brief Build and send a framed message: #CMD DATA CS !
 */
static void send_frame(const struct device *dev, char cmd, const char *data, size_t data_len)
{
    uint8_t frame[1U + 1U + UART_BUF_SIZE + 3U + 1U];
    size_t pos = 0U;

    frame[pos++] = '#';
    frame[pos++] = (uint8_t)cmd;
    for (size_t i = 0U; i < data_len; i++) {
        frame[pos++] = (uint8_t)data[i];
    }
    /* Compute checksum over CMD and DATA */
    uint8_t cs = calculate_checksum(&frame[1], 1U + data_len);
    /* Encode checksum as three ASCII digits */
    frame[pos++] = '0' + (uint8_t)((cs / 100U) % 10U);
    frame[pos++] = '0' + (uint8_t)((cs / 10U) % 10U);
    frame[pos++] = '0' + (uint8_t)(cs % 10U);
    frame[pos++] = '!';

    send_bytes(dev, frame, pos);
}

/**
 * @brief Send acknowledgment: E<code>!
 */
static void send_ack(const struct device *dev, char code)
{
    send_frame(dev, 'E', &code, 1U);
}

/**
 * @brief Parse and handle a received frame
 */
static void handle_command(const struct device *dev, const uint8_t *buf, size_t len)
{
    /* Minimum frame length = # + CMD + CS(3) + ! */
    if (len < 6U) {
        send_ack(dev, 'f'); /* framing error */
        return;
    }
    /* Check start/end */
    if ((buf[0] != '#') || (buf[len - 1] != '!')) {
        send_ack(dev, 'f');
        return;
    }
    char cmd = (char)buf[1];
    /* Extract checksum string */
    char cs_str[4] = { (char)buf[len-4], (char)buf[len-3], (char)buf[len-2], '\0' };
    uint8_t cs_rcv = (uint8_t)atoi(cs_str);
    /* Calculate data length */
    size_t data_len = len - 6U;
    const uint8_t *data_ptr = &buf[2];
    /* Verify checksum */
    uint8_t cs_calc = calculate_checksum((const uint8_t *)&buf[1], 1U + data_len);
    if (cs_calc != cs_rcv) {
        send_ack(dev, 's'); /* checksum error */
        return;
    }
    /* Handle commands */
    switch (cmd) {
    case 'M': { /* Set max temperature: DATA = 3-digit */
        if (data_len != 3U) {
            send_ack(dev, 'i'); /* invalid */
        } else {
            char temp_str[4] = { (char)data_ptr[0], (char)data_ptr[1], (char)data_ptr[2], '\0' };
            int max_temp = atoi(temp_str);
            rtdb_set_max_temp((int16_t)max_temp);
            send_ack(dev, 'o'); /* OK */
        }
        break;
    }
    case 'C': { /* Query current temperature */
        int cur = rtdb_get_current_temp();
        if (cur < 0) {
            cur = 0;
        } else if (cur > 999) {
            cur = 999;
        }
        char temp_out[4];
        temp_out[0] = (char)('0' + (cur / 100));
        temp_out[1] = (char)('0' + ((cur / 10) % 10));
        temp_out[2] = (char)('0' + (cur % 10));
        temp_out[3] = '\0';
        send_frame(dev, 'c', temp_out, 3U);
        break;
    }
    case 'S': { /* Set controller parameters (stub) */
        /* TODO: parse and apply controller settings */
        send_ack(dev, 'o'); /* OK */
        break;
    }
    default:
        send_ack(dev, 'i'); /* invalid command */
        break;
    }
}
