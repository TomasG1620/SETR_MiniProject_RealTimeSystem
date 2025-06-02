# ==================================================================
# thermalcontrolproject/tests/CMakeLists.txt
# ==================================================================
cmake_minimum_required(VERSION 3.20.0)

# 1) Encontra Zephyr (para usar native_posix toolchain)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(thermalcontrol_tests)

# 2) Inclui diretórios onde estão os headers originais (para #include)
#    Ajuste se seus headers estiverem em “src/” ou “include/”.
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# 3) Define uma macro UNIT_TEST para que possamos condicionar trechos de hardware
#    (ex: #ifdef UNIT_TEST) sem alterar os arquivos originais.
zephyr_compile_definitions(UNIT_TEST=1)

# 4) Trazer a fonte do Unity
file(GLOB UNITY_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/unity/unity.c
)

# 5) Coleta TODOS os arquivos test_*.c
file(GLOB TEST_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/test_*.c
)

# 6) Adiciona os módulos “originais” que queremos testar INDEPENDENTEMENTE de main.c:
#    - ../src/rtdb.c
#    - ../src/controller.c
#    - ../src/uartcomm.c
#
#  IMPORTANTE: não incluímos main.c nem o hardware de Zephyr. Faremos “stubs” nos testes.
set(SRC_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/rtdb.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/controller.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/uartcomm.c
)

# 7) Construir o executável de teste “run_tests”
add_executable(run_tests
    ${UNITY_SRC}
    ${TEST_SRC}
    ${SRC_MODULES}
)

# 8) Linkar com a biblioteca de Zephyr (native_posix) para satisfazer dependências mínimas
target_link_libraries(run_tests PRIVATE zephyr)
