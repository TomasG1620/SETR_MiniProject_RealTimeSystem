/**
 * @file rtdb.c
 * @brief Real-Time Database (RTDB) para o controlador térmico
 *
 * @details
 *   Esta implementação mantém um conjunto de variáveis de estado e configuração
 *   que são partilhadas entre várias threads:
 *     - system_on       (bool): sistema ligado/desligado
 *     - setpoint        (int16): temperatura alvo (°C)
 *     - current_temp    (int16): temperatura lida do sensor (°C)
 *     - max_temp        (int16): temperatura máxima permitida (°C)
 *     - min_temp        (int16): temperatura mínima permitida (°C)
 *     - sampling_rate_ms(uint32): intervalo de amostragem do sensor (ms)
 *
 *   Todas as funções de acesso à RTDB (getters e setters) protegem a região crítica
 *   usando um mutex (k_mutex). O mutex é inicializado via SYS_INIT() logo no arranque.
 *
 * @note
 *   - setpoint nunca ultrapassa max_temp nem fica abaixo de min_temp.
 *   - min_temp e max_temp atualizam o setpoint caso este fique fora dos limites.
 */

 #include "rtdb.h"
 #include <zephyr/kernel.h>
 
 /**
  * @brief Estrutura interna que guarda todos os valores do RTDB
  */
 static rtdb_t g_rtdb = {
     .system_on        = true,    /* Inicialmente, sistema ligado */
     .setpoint         = 26,      /* Setpoint padrão: 26°C */
     .current_temp     = 0,       /* Temperatura inicial (valor dummy) */
     .max_temp         = 80,      /* Valor máximo inicial: 80°C */
     .min_temp         = 20,      /* Valor mínimo inicial: 20°C */
     .sampling_rate_ms = 1000     /* Intervalo de 1 segundo */
 };
 
 static struct k_mutex rtdb_mutex; 
 
 /**
  * @brief Inicializa o mutex do RTDB antes de qualquer acesso
  *
  * Chamado automaticamente pela macro SYS_INIT(), no nível APPLICATION.
  *
  * @param dev  Ponteiro para dispositivo (não utilizado)
  * @return     0 sempre
  */
 static int rtdb_mutex_init(const struct device *dev)
 {
     ARG_UNUSED(dev);
     k_mutex_init(&rtdb_mutex);
     return 0;
 }
 SYS_INIT(rtdb_mutex_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
 
 /**
  * @brief Lê o valor de system_on (protected by mutex)
  *
  * @return true se sistema ligado, false se desligado
  */
 bool rtdb_get_system_on(void)
 {
     bool v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.system_on;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza o valor de system_on (protected by mutex)
  *
  * @param on  true para ligar sistema, false para desligar
  */
 void rtdb_set_system_on(bool on)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     g_rtdb.system_on = on;
     k_mutex_unlock(&rtdb_mutex);
 }
 
 /**
  * @brief Lê o setpoint atual (protected by mutex)
  *
  * @return Setpoint (°C)
  */
 int16_t rtdb_get_setpoint(void)
 {
     int16_t v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.setpoint;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza o setpoint, limitando entre min_temp e max_temp (protected by mutex)
  *
  * @param val  Novo valor de setpoint (°C)
  */
 void rtdb_set_setpoint(int16_t val)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     if (val > g_rtdb.max_temp) {
         g_rtdb.setpoint = g_rtdb.max_temp;
     } else if (val < g_rtdb.min_temp) {
         g_rtdb.setpoint = g_rtdb.min_temp;
     } else {
         g_rtdb.setpoint = val;
     }
     k_mutex_unlock(&rtdb_mutex);
 }
 
 /**
  * @brief Lê a temperatura atual (protected by mutex)
  *
  * @return current_temp (°C)
  */
 int16_t rtdb_get_current_temp(void)
 {
     int16_t v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.current_temp;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza current_temp (protected by mutex)
  *
  * @param val  Valor de temperatura lido do sensor (°C, complemento a dois)
  */
 void rtdb_set_current_temp(int16_t val)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     g_rtdb.current_temp = val;
     k_mutex_unlock(&rtdb_mutex);
 }
 
 /**
  * @brief Lê max_temp (protected by mutex)
  *
  * @return max_temp (°C)
  */
 int16_t rtdb_get_max_temp(void)
 {
     int16_t v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.max_temp;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza max_temp; ajusta setpoint se estiver acima de max_temp (protected by mutex)
  *
  * @param val  Novo valor máximo permitido (°C)
  */
 void rtdb_set_max_temp(int16_t val)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     g_rtdb.max_temp = val;
     if (g_rtdb.setpoint > g_rtdb.max_temp) {
         g_rtdb.setpoint = g_rtdb.max_temp;
     }
     k_mutex_unlock(&rtdb_mutex);
 }
 
 /**
  * @brief Lê min_temp (protected by mutex)
  *
  * @return min_temp (°C)
  */
 int16_t rtdb_get_min_temp(void)
 {
     int16_t v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.min_temp;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza min_temp; ajusta setpoint se estiver abaixo de min_temp (protected by mutex)
  *
  * @param val  Novo valor mínimo permitido (°C)
  */
 void rtdb_set_min_temp(int16_t val)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     g_rtdb.min_temp = val;
     if (g_rtdb.setpoint < g_rtdb.min_temp) {
         g_rtdb.setpoint = g_rtdb.min_temp;
     }
     k_mutex_unlock(&rtdb_mutex);
 }
 
 /**
  * @brief Lê sampling_rate_ms (protected by mutex)
  *
  * @return Intervalo de amostragem em milissegundos
  */
 uint32_t rtdb_get_sampling_rate(void)
 {
     uint32_t v;
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
     v = g_rtdb.sampling_rate_ms;
     k_mutex_unlock(&rtdb_mutex);
     return v;
 }
 
 /**
  * @brief Atualiza sampling_rate_ms, limitando entre 10 ms e 60000 ms (protected by mutex)
  *
  * @param ms  Novo intervalo de amostragem em milissegundos
  */
 void rtdb_set_sampling_rate(uint32_t ms)
 {
     k_mutex_lock(&rtdb_mutex, K_FOREVER);
 
     if (ms < 10) {
         g_rtdb.sampling_rate_ms = 10;
     } else if (ms > 60000) {
         g_rtdb.sampling_rate_ms = 60000;
     } else {
         g_rtdb.sampling_rate_ms = ms;
     }
     k_mutex_unlock(&rtdb_mutex);
 }

