#ifndef UARTCOMM_DUMMY_H
#define UARTCOMM_DUMMY_H

#include <stddef.h>
#include <stdint.h>

/* Cálculo de checksum módulo-256 */
uint8_t calculate_checksum(const uint8_t *buf, size_t len);

/* Trata de um frame completo */
void handle_command(const uint8_t *buf, size_t len);

/* Capturam a saída “virtual” da UART (nos testes) */
void clear_uart_test_output(void);
const char *get_uart_test_output(void);

#endif /* UARTCOMM_DUMMY_H */

