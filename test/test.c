/*  Assignment 2 - Sistemas Embutidos e de Tempo Real
---------------------------------------------------------------------------------------------
Autores: Tomás Gomes   [98807]
         Vasco Pestana [88827]

Descrição:
        -> Implementação de um módulo em C que processa comandos que são recebidos em UART 
        (um caratér de cada vez). O módulo faz parte de um sensor inteligente, capaz de medir
        temperatura(-50ºC ... 60ºC), humidade(0 ... 100%) e CO2 (400 ... 20000 ppm).
*/

/** 
 * @file main.c
 * @brief Programa principal para o processador de comandos de sensores inteligentes.
 * 
 * Este programa inicializa o processador de comandos e executa exemplos de comandos válidos 
 * com diferentes funcionalidades, como leitura de sensores e histórico.
 * 
 * @authors 
 *      - Tomás Gomes [98807]  
 *      - Vasco Pestana [88827]
 * @date 2025
 */

#include <stdio.h>
#include <string.h>

#include "Command_Processor.h"

int main(void) {
    // Inicializar o processador de comandos
    CommandProcessorInit();

    // -------------------------
    // Simular comando: #A X Y ! (todos os sensores)
    // Comando: A (All), Data: T (qualquer, pois 'A' ignora), Checksum: A + T
    // -------------------------
    char cmd = 'A';
    char data = 'T';  // dummy
    char checksum = CalculateChecksum(cmd, data);

    char comando1[] = { '#', cmd, data, checksum, '!' };
    printf("\n[TEST 1] Command 'A' - All sensors\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando1[i]);
    }

    // -------------------------
    // Simular comando: #P T X ! (sensor específico - Temperatura)
    // -------------------------
    cmd = 'P';
    data = 'T';  // Temperatura
    checksum = CalculateChecksum(cmd, data);

    char comando2[] = { '#', cmd, data, checksum, '!' };
    printf("\n[TEST 2] Command 'P T' - Temperature\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando2[i]);
    }

    // -------------------------
    // Simular comando: #P H X ! (sensor específico - Humidade)
    // -------------------------
    cmd = 'P';
    data = 'H';  // Humidade
    checksum = CalculateChecksum(cmd, data);

    char comando3[] = { '#', cmd, data, checksum, '!' };
    printf("\n[TEST 3] Command 'P H' - Humidity\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando3[i]);
    }

    // -------------------------
    // Simular comando: #L X X ! (listar histórico)
    // -------------------------
    cmd = 'L';
    data = 'X';  // qualquer valor
    checksum = CalculateChecksum(cmd, data);

    char comando4[] = { '#', cmd, data, checksum, '!' };
    printf("\n[TEST 4] Command 'L' - Last 20 samples\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando4[i]);
    }

    // -------------------------
    // Simular comando: #R X X ! (reset no histórico)
    // -------------------------
    cmd = 'R';
    data = 'X';  // qualquer valor
    checksum = CalculateChecksum(cmd, data);

    char comando5[] = { '#', cmd, data, checksum, '!' };
    printf("\n[TEST 5] Command 'R' - Reset\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando5[i]);
    }

    // -------------------------
    // Simular comando inválido (checksum errado)
    // -------------------------
    char comando6[] = { '#', 'P', 'T', 0x00, '!' };  // checksum incorreto
    printf("\n[TEST 6] Invalid Command (wrong checksum)\n");
    for (int i = 0; i < 5; i++) {
        UART_ReceiveChar(comando6[i]);
    }

    return 0;
}

