/**
 * @file uartcomm.c
 * @brief Módulo de comunicação UART: parser de frames e framing
 *
 * @details
 *   - Usa polling da UART (uart_poll_in/uart_poll_out) para receber bytes.
 *   - Implementa framing: “# <CMD> <DATA ASCII> <CS(3 dígitos)> !”
 *   - Verifica framing e checksum. Envia acknowledgment via send_ack() ou resposta de consulta.
 *   - Suporta os seguintes comandos:
 *       • #MxxxYYY! → set max_temp (3 dígitos); envia ACK 'o' ou 'i'
 *       • #mxxxYYY! → set min_temp (3 dígitos); envia ACK 'o' ou 'i'
 *       • #C!       → get current_temp; envia #cXXXYYY!
 *       • #RxxxxYYY!→ set sampling_rate (4 dígitos); envia ACK 'o' ou 'i'
 *       • #r!       → get sampling_rate; envia #sXXXXYYY!
 *       • #E0!/#E1! → liga/desliga sistema; envia ACK 'o' ou 'i'
 *       • #S…!      → set parâmetros do controlador (stub); envia ACK 'o' ou 'i'
 *
 *   - Erros:
 *       • framing error → ACK com código 'f'
 *       • checksum error → ACK com código 's'
 *       • invalid command → ACK com código 'i'
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
 #define UART_PRIORITY   5U     /**< Prioridade da thread UART */
 #define UART_BUF_SIZE   64U    /**< Tamanho do buffer de receção de bytes */
 
 /**
  * @brief Calcula checksum (módulo-256) sobre os len primeiros bytes de buf
  *
  * @param buf   Array de bytes a considerar (CMD + DATA)
  * @param len   Número de bytes a considerar no cálculo
  * @return      Checksum (soma & 0xFF) em uint8_t
  */
 static uint8_t calculate_checksum(const uint8_t *buf, size_t len);
 
 /**
  * @brief Envia raw bytes pela UART usando polling
  *
  * @param dev   Dispositivo UART
  * @param data  Ponteiro para array de bytes
  * @param len   Número de bytes a enviar
  */
 static void send_bytes(const struct device *dev, const uint8_t *data, size_t len);
 
 /**
  * @brief Constroi e envia um frame pela UART:
  *        # <cmd> <data ASCII> <CS(3 dígitos)> !
  *
  * @param dev       Dispositivo UART
  * @param cmd       Carácter de comando (e.g. 'E', 'c', 'M', 'm', etc.)
  * @param data      Ponteiro para string ASCII contendo o payload (sem '\0')
  * @param data_len  Comprimento de data (número de bytes ASCII)
  */
 static void send_frame(const struct device *dev, char cmd, const char *data, size_t data_len);
 
 /**
  * @brief Envia um ACK simples pela UART: #E<code>!
  *
  * @param dev   Dispositivo UART
  * @param code  'o' = OK, 'f' = framing error, 's' = checksum error, 'i' = invalid
  */
 static void send_ack(const struct device *dev, char code);
 
 /**
  * @brief Trata um frame completo recebido em buf[0..len-1], onde buf[0]=='#' e buf[len-1]=='!'
  *
  *  - CMD = buf[1]
  *  - DATA = buf[2..(2+data_len-1)]
  *  - CS_str = buf[len-4..len-2] (3 dígitos ASCII)
  *  - data_len = len - 6   (descarta '#', CMD, CS(3), '!')
  *
  *  Suporta:
  *   - 'M': #MxxxYYY!  → set max_temp
  *   - 'm': #mxxxYYY!  → set min_temp
  *   - 'C': #C!        → consulta current_temp
  *   - 'R': #RxxxxYYY! → set sampling_rate (4 dígitos)
  *   - 'r': #r!        → get sampling_rate (4 dígitos)
  *   - 'E': #E0!/#E1!  → liga/desliga sistema
  *   - 'S': #S…!       → set parâmetros do controlador (stub)
  *
  *  Em caso de:
  *   - framing error → envia send_ack(dev, 'f')
  *   - invalid command → envia send_ack(dev, 'i')
  *   - checksum error → envia send_ack(dev, 's')
  *
  *  Se houver mais de um erro (p.ex. checksum e inválido), envia ambos: primeiro 's', depois 'i'.
  *
  * @param dev   Dispositivo UART
  * @param buf   Buffer que contém o frame completo
  * @param len   Comprimento do frame (bytes)
  */
 static void handle_command(const struct device *dev, const uint8_t *buf, size_t len);
 
 /**
  * @brief Thread de polling na UART que enquadra bytes recebidos e chama handle_command()
  *
  *   - Lê bytes um a um via uart_poll_in()
  *   - Implementa máquina de estados simples:
  *       1) Ignora CR/LF fora de frame
  *       2) Se recebe '!' sem ter visto '#' primeiro → framing error
  *       3) Se recebe '#' dentro de frame não fechado → framing error no frame anterior
  *       4) Se '#' e idx==0 → começa novo frame
  *       5) Enquanto idx>0, acumula bytes até encontrar '!' ou encher buffer
  *       6) Se encher buffer sem '!' → framing error
  *
  *   - Se encontra '!' dentro de frame, chama handle_command(uart_dev, buf, idx)
  *
  * @param p1  Não utilizado
  * @param p2  Não utilizado
  * @param p3  Não utilizado
  */
 static void uart_task(void *p1, void *p2, void *p3);
 
 K_THREAD_STACK_DEFINE(uart_stack, UART_STACK_SIZE); 
 static struct k_thread uart_thread_data;             
 
 /**
  * @brief Inicializa a comunicação UART criando a thread uart_task()
  */
 void uart_comm_init(void)
 {
     k_thread_create(&uart_thread_data, uart_stack, UART_STACK_SIZE,
                     uart_task, NULL, NULL, NULL,
                     UART_PRIORITY, 0, K_NO_WAIT);
 }
 
 static uint8_t calculate_checksum(const uint8_t *buf, size_t len)
 {
     uint16_t sum = 0U;
     for (size_t i = 0U; i < len; i++) {
         sum += buf[i];
     }
     return (uint8_t)(sum & 0xFFU);
 }
 
 static void send_bytes(const struct device *dev, const uint8_t *data, size_t len)
 {
     for (size_t i = 0U; i < len; i++) {
         uart_poll_out(dev, data[i]);
     }
 }
 
 static void send_frame(const struct device *dev, char cmd, const char *data, size_t data_len)
 {
     /* 1 byte ('#') + 1 byte(cmd) + data_len + 3 bytes(checksum) + 1 byte('!') */
     uint8_t frame[1U + 1U + UART_BUF_SIZE + 3U + 1U];
     size_t  pos = 0U;
 
     frame[pos++] = '#';
     frame[pos++] = (uint8_t)cmd;
     for (size_t i = 0U; i < data_len; i++) {
         frame[pos++] = (uint8_t)data[i];
     }
     /* Calcula checksum [CMD] + [DATA] */
     uint8_t cs = calculate_checksum(&frame[1], 1U + data_len);
     /* Converte checksum para 3 dígitos ASCII */
     frame[pos++] = '0' + (uint8_t)((cs / 100U) % 10U);
     frame[pos++] = '0' + (uint8_t)((cs / 10U) % 10U);
     frame[pos++] = '0' + (uint8_t)(cs % 10U);
 
     frame[pos++] = '!';
     send_bytes(dev, frame, pos);
 }
 
 static void send_ack(const struct device *dev, char code)
 {
     /* ACK genérico: #E<code>! */
     send_frame(dev, 'E', &code, 1U);
 }
 
 static void handle_command(const struct device *dev, const uint8_t *buf, size_t len)
 {
     /* Tamanho mínimo = 6 bytes: # + CMD + CS(3) + ! */
     if (len < 6U) {
         send_ack(dev, 'f');  /* framing error */
         return;
     }
     /* Verifica '#' no início e '!' no fim */
     if ((buf[0] != '#') || (buf[len - 1] != '!')) {
         send_ack(dev, 'f');
         return;
     }
     /* Extrai CMD */
     char cmd = (char)buf[1];
 
     /* Extrai checksum recebido (3 dígitos ASCII) */
     char cs_str[4] = {
         (char)buf[len - 4],
         (char)buf[len - 3],
         (char)buf[len - 2],
         '\0'
     };
     uint8_t cs_rcv = (uint8_t)atoi(cs_str);
 
     /* Determina tamanho de DATA */
     size_t data_len = len - 6U;  /* retira '#', CMD, CS(3), '!' */
     const uint8_t *data_ptr = &buf[2];
 
     /* Verifica se o comando é reconhecido */
     bool cmd_valido = (cmd == 'M') || (cmd == 'm') || (cmd == 'C') ||
                       (cmd == 'R') || (cmd == 'r') || (cmd == 'E') || (cmd == 'S');
     if (!cmd_valido) {
         /* Comando desconhecido: compara checksum isolado de CMD */
         uint8_t cs_cmd = (uint8_t)cmd;
         if (cs_cmd != cs_rcv) {
             send_ack(dev, 's');  /* checksum error */
             send_ack(dev, 'i');  /* invalid command */
         } else {
             send_ack(dev, 'i');  
         }
         return;
     }
 
     /* Verifica checksum completo [CMD + DATA] */
     uint8_t cs_calc = calculate_checksum((const uint8_t *)&buf[1], 1U + data_len);
     if (cs_calc != cs_rcv) {
         send_ack(dev, 's');  /* checksum error */
         return;
     }
 
     switch (cmd) {
         case 'M': {  /* #MxxxYYY! → set max temperature */
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
                 if (val < rtdb_get_min_temp()) {
                     /* Não permitir max < min */
                     send_ack(dev, 'i');
                 } else {
                     rtdb_set_max_temp((int16_t)val);
                     printk("[UART] max_temp atualizado para %d°C\n", val);
                     send_ack(dev, 'o');
                 }
             }
             break;
         }
         case 'm': {  /* #mxxxYYY! → set min temperature */
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
                     /* Não permitir min > max */
                     send_ack(dev, 'i');
                 } else {
                     rtdb_set_min_temp((int16_t)val);
                     printk("[UART] min_temp atualizado para %d°C\n", val);
                     send_ack(dev, 'o');
                 }
             }
             break;
         }
         case 'C': {  /* #C! → consulta current temperature */
             int cur = rtdb_get_current_temp();
             /* Limita a 0..999 para caber em 3 dígitos */
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
         case 'R': {  /* #RxxxxYYY! → set samplingRate em ms (0000..9999) */
             if (data_len != 4U) {
                 send_ack(dev, 'i');
             } else {
                 char tmp[5] = {
                     (char)data_ptr[0],
                     (char)data_ptr[1],
                     (char)data_ptr[2],
                     (char)data_ptr[3],
                     '\0'
                 };
                 int val = atoi(tmp);
                 if (val < 10 || val > 9999) {
                     send_ack(dev, 'i');
                 } else {
                     rtdb_set_sampling_rate((uint32_t)val);
                     printk("[UART] sampling_rate atualizado para %d ms\n", val);
                     send_ack(dev, 'o');
                 }
             }
             break;
         }
         case 'r': {  /* #r! → get samplingRate (4 dígitos) */
             if (data_len != 0U) {
                 send_ack(dev, 'i');
             } else {
                 uint32_t sr = rtdb_get_sampling_rate();
                 if (sr > 9999U) {
                     sr = 9999U;
                 }
                 char out[5] = {
                     (char)('0' + ((sr / 1000) % 10)),
                     (char)('0' + ((sr / 100)  % 10)),
                     (char)('0' + ((sr / 10)   % 10)),
                     (char)('0' + (sr % 10)),
                     '\0'
                 };
                 send_frame(dev, 's', out, 4U);
             }
             break;
         }
         case 'E': {  /* #E0! → liga sistema; #E1! → desliga sistema */
             if (data_len != 1U) {
                 send_ack(dev, 'i');
             } else {
                 char c = (char)data_ptr[0];
                 if (c == '0') {
                     rtdb_set_system_on(true);
                     printk("[UART] Sistema ligado via comando #E0\n");
                     send_ack(dev, 'o');
                 } else if (c == '1') {
                     rtdb_set_system_on(false);
                     printk("[UART] Sistema desligado via comando #E1\n");
                     send_ack(dev, 'o');
                 } else {
                     /* Payload diferente de '0' ou '1' → invalid */
                     send_ack(dev, 'i');
                 }
             }
             break;
         }
         case 'S': {  /* #Sxxx...xxxYYY! → set controller parameters */
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
             /* Nunca deve chegar aqui */
             send_ack(dev, 'i');
             break;
     }
 }
 
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
 
             if ((byte == '\r') || (byte == '\n')) {
                 continue;  /* descarta CR/LF antes de começar/continuar um frame */
             }
 
             /* Se byte == '!' e idx == 0 → framing error imediato */
             if ((byte == '!') && (idx == 0U)) {
                 send_ack(uart_dev, 'f');
                 continue;
             }
 
             /* Se byte == '#' e idx > 0 → framing error no frame anterior */
             if ((byte == '#') && (idx > 0U)) {
                 send_ack(uart_dev, 'f');
                 idx = 0U;
                 buf[idx++] = '#';
                 continue;
             }
 
             /* Se ENTER no meio de frame (idx > 0) → framing error */
             if (((byte == '\n') || (byte == '\r')) && (idx > 0U)) {
                 send_ack(uart_dev, 'f');
                 idx = 0U;
                 continue;
             }
 
             /* Se for '#' e idx == 0 → começa novo frame */
             if (byte == '#') {
                 idx = 0U;
                 buf[idx++] = byte;
                 continue;
             }
 
             /* Se dentro de um frame (idx > 0), acumula bytes até achar '!' ou encher buffer */
             if (idx > 0U) {
                 buf[idx++] = byte;
 
                 /* Se for '!' → fim de frame */
                 if (byte == '!') {
                     handle_command(uart_dev, buf, idx);
                     idx = 0U;
                     continue;
                 }
 
                 /* Se buffer encheu sem ver '!' → framing error */
                 if (idx >= UART_BUF_SIZE) {
                     send_ack(uart_dev, 'f');
                     idx = 0U;
                     continue;
                 }
 
                 /* Senão, continua a acumular bytes do frame */
                 continue;
             }
 
             /* 6) Qualquer outro byte fora de frame (idx==0 e não é nem '!' nem '#') → ignora */
         }
 
         k_sleep(K_MSEC(10));
     }
 }
 