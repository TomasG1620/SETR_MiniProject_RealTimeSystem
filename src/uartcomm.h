#ifndef UARTCOMM_H
#define UARTCOMM_H

#include <zephyr/device.h>

/**
 * @file uartcomm.h
 * @brief Interface do módulo de comunicação UART (parser de comandos + framing)
 *
 * @details
 *   Este header exporta apenas a função uart_comm_init(), que inicia uma thread
 *   responsável por ler bytes da UART em modo polling, reconstituir frames do
 *   tipo “#<CMD><DATA><CS>!” e disparar o tratamento de cada comando.
 */

/**
 * @brief Inicializa a thread de comunicação UART
 *
 * Cria uma thread de prioridade 5 que roda uart_task(), fazendo polling da UART,
 * montando e validando frames, e chamando internamente handle_command() para processar
 * cada comando recebido.
 */
void uart_comm_init(void);

#endif /* UARTCOMM_H */

