#ifndef CONTROLLER_H
#define CONTROLLER_H

/**
 * @file controller.h
 * @brief Interface do controlador On/Off para processo térmico
 *
 * @details
 *   Proporciona a função controller_init(), que cria uma thread responsável
 *   por ler o setpoint e a temperatura atual da RTDB e controlar um MOSFET
 *   com histerese ±1°C.
 */

/**
 * @brief Inicializa o on/off heater controller
 *
 * Esta função:
 *   1. Lê o nó DT “gpio1” e configura o pino P1.12 como saída (nível alto = heater OFF).
 *   2. Cria uma thread (priority=5, stack=1KB) que roda control_task() ciclicamente.
 */
void controller_init(void);

#endif /* CONTROLLER_H */

