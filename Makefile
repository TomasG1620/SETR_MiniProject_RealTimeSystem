# Makefile simplificado para rodar os testes Unity com os módulos “dummy”

CC       := gcc
CFLAGS   := -Wall -Wextra -std=c99 -DUNIT_TEST -Idummy -IUnity/src
UNITY_SRC := Unity/src/unity.c
RTDB_D    := dummy/rtdb_dummy.c
CTRL_D    := dummy/controller_dummy.c
UART_D    := dummy/uartcomm_dummy.c

all: test_rtdb test_controller test_uartcomm

test_rtdb: $(RTDB_D) $(UNITY_SRC) tests/test_rtdb.c
	$(CC) $(CFLAGS) $^ -o test_rtdb

test_controller: $(RTDB_D) $(CTRL_D) $(UNITY_SRC) tests/test_controller.c
	$(CC) $(CFLAGS) $^ -o test_controller

test_uartcomm: $(RTDB_D) $(UART_D) $(UNITY_SRC) tests/test_uartcomm.c
	$(CC) $(CFLAGS) $^ -o test_uartcomm

clean:
	rm -f test_rtdb test_controller test_uartcomm

.PHONY: all clean

