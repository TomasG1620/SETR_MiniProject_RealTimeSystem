#ifndef RTDB_H
#define RTDB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file rtdb.h
 * @brief Protótipos do Real-Time Database (RTDB) para o controlador térmico
 *
 * @details
 *   Este ficheiro define a estrutura rtdb_t e as funções de acesso (getters/setters)
 *   protegidas por mutex, de modo a permitir comunicação segura entre várias tasks.
 */

/**
 * @brief Estrutura que contém todas as variáveis compartilhadas no sistema
 */
typedef struct {
    bool    system_on;         /* Sistema ligado/desligado */
    int16_t setpoint;          /* Temperatura alvo (°C) */
    int16_t current_temp;      /* Temperatura lida do sensor (°C) */
    int16_t max_temp;          /* Temperatura máxima permitida (°C) */
    int16_t min_temp;          /* Temperatura mínima permitida (°C) */
    uint32_t sampling_rate_ms; /* Intervalo de amostragem em ms */
} rtdb_t;

/**
 * @brief Lê se o sistema está ligado ou não
 * @return true se ligado, false se desligado
 */
bool    rtdb_get_system_on(void);

/**
 * @brief Altera o estado do sistema (ligado/desligado)
 * @param on  true para ligar, false para desligar
 */
void    rtdb_set_system_on(bool on);

/**
 * @brief Lê o setpoint atual (°C)
 * @return Valor do setpoint (°C)
 */
int16_t rtdb_get_setpoint(void);

/**
 * @brief Define um novo setpoint, validando entre min_temp e max_temp
 * @param val  Valor de setpoint desejado (°C)
 */
void    rtdb_set_setpoint(int16_t val);

/**
 * @brief Lê a temperatura atual lida do sensor (°C)
 * @return Valor de current_temp (°C)
 */
int16_t rtdb_get_current_temp(void);

/**
 * @brief Atualiza a temperatura atual (°C)
 * @param val  Valor lido do sensor (°C)
 */
void    rtdb_set_current_temp(int16_t val);

/**
 * @brief Lê o valor de temperatura máxima permitida (°C)
 * @return max_temp (°C)
 */
int16_t rtdb_get_max_temp(void);

/**
 * @brief Define um novo valor de temperatura máxima, ajustando setpoint se necessário
 * @param val  Valor máximo permitido (°C)
 */
void    rtdb_set_max_temp(int16_t val);

/**
 * @brief Lê o valor de temperatura mínima permitida (°C)
 * @return min_temp (°C)
 */
int16_t rtdb_get_min_temp(void);

/**
 * @brief Define um novo valor de temperatura mínima, ajustando setpoint se necessário
 * @param val  Valor mínimo permitido (°C)
 */
void    rtdb_set_min_temp(int16_t val);

/**
 * @brief Lê o intervalo de amostragem do sensor (ms)
 * @return sampling_rate_ms
 */
uint32_t rtdb_get_sampling_rate(void);

/**
 * @brief Define o intervalo de amostragem do sensor (limitado a 10..60000 ms)
 * @param ms  Novo intervalo em milissegundos
 */
void     rtdb_set_sampling_rate(uint32_t ms);

#endif /* RTDB_H */

