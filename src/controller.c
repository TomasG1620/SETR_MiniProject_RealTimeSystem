/**
 * @file controller.c
 * @brief On/Off controller para processo térmico
 *
 * @details
 *   - Lê setpoint e current_temp da RTDB
 *   - Controla um MOSFET (porta P1.12) com histerese ±1 °C
 *   - Quando sistema está desligado (system_on = false), garante que o aquecedor fique OFF
 *
 *   O MOSFET é assumido como “active-low” (nível lógico 0 = heater ON, 1 = heater OFF).
 */

 #include "controller.h"
 #include "rtdb.h"
 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/gpio.h>
 #include <zephyr/sys/printk.h>
 
 #define HEATER_GPIO_NODE DT_NODELABEL(gpio1)  
 #define HEATER_PIN       12U                  /* P1.12 ligado à porta do MOSFET */
 
 static const struct device *heater_dev; 
 static K_THREAD_STACK_DEFINE(ctrl_stack, 1024);  
 static struct k_thread ctrl_thread;              
 
 /**
  * @brief Lógica de controlo On/Off com histerese ±1°C
  *
  * Quando o sistema está desligado (system_on == false), o aquecedor é forçado a OFF.
  * Caso contrário:
  *   - Se current_temp ≤ setpoint − 1°C → liga aquecedor
  *   - Se current_temp ≥ setpoint + 1°C → desliga aquecedor
  *   - Se estiver entre (setpoint − 1, setpoint + 1) mantém o estado anterior
  *
  * @param p1  Não utilizado
  * @param p2  Não utilizado
  * @param p3  Não utilizado
  */
 static void control_task(void *p1, void *p2, void *p3)
 {
     ARG_UNUSED(p1);
     ARG_UNUSED(p2);
     ARG_UNUSED(p3);
 
     bool heater = false;  /* Estado atual do aquecedor */
 
     for (;;)
     {
         bool system_on = rtdb_get_system_on();
         int16_t sp     = rtdb_get_setpoint();
         int16_t cur    = rtdb_get_current_temp();
 
         if (!system_on) {
             /* Se o sistema estiver desligado, garante que aquecedor fique desligado */
             heater = false;
         } else {
             /* Histerese ±1°C em torno do setpoint */
             if (cur <= sp - 1) {
                 heater = false;
             } else if (cur >= sp + 1) {
                 heater = true;
             }
             /* Caso contrário (entre sp-1 e sp+1), mantém heater_on inalterado */
         }
 
         /* Active-low gate: 0 = ON, 1 = OFF */
         gpio_pin_set(heater_dev, HEATER_PIN, heater ? 0 : 1);
 
         printk("[Ctrl] sp=%d°C cur=%d°C heater=%s\n",
                sp, cur, heater ? "OFF" : "ON");
 
         k_sleep(K_MSEC(2000));
     }
 }
 
 /**
  * @brief Inicializa o controlador ON/OFF
  *
  *   - Obtém o dispositivo GPIO (P1.12) para o MOSFET
  *   - Configura P1.12 como saída com nível alto (heater OFF)
  *   - Cria a thread control_task com prioridade 5
  */
 void controller_init(void)
 {
     heater_dev = DEVICE_DT_GET(HEATER_GPIO_NODE);
     __ASSERT(heater_dev != NULL, "Heater GPIO device not found");
     if (!device_is_ready(heater_dev)) {
         printk("[Ctrl] Heater GPIO não pronto\n");
         return;
     }
 
     /* Configura P1.12 como saída, nível alto (desliga o heater) */
     gpio_pin_configure(heater_dev, HEATER_PIN, GPIO_OUTPUT_INACTIVE);
     gpio_pin_set(heater_dev, HEATER_PIN, 1);
 
     /* Lança a thread de controlo */
     k_thread_create(&ctrl_thread, ctrl_stack, K_THREAD_STACK_SIZEOF(ctrl_stack),
                     control_task, NULL, NULL, NULL,
                     5, 0, K_NO_WAIT);
     printk("[Init] Controller\n");
 }
 
