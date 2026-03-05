cmake_minimum_required(VERSION 3.16)
project(bq24074_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

# Stubs for ESP-IDF types
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(test_bq24074_config
  test_bq24074_config.c
)

add_test(NAME bq24074_config_tests COMMAND test_bq24074_config)
