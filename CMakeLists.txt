cmake_minimum_required(VERSION 3.16)
project(DMA_UART_Framework C ASM)

# ================================================================
# Target: STM32F103ZET6
# ================================================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---- MCU flags ----
set(CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CPU_FLAGS} -Wall -Wextra -O2")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${CPU_FLAGS}")

add_definitions(-DSTM32F103xE -DUSE_HAL_DRIVER)

# ---- Source files ----
set(SOURCES
    ringbuffer/ringbuffer.c
    port/stm32f1xx/uart_port.c
    driver/uart_dma.c
    api/uart_api.c
)

# ---- Include paths (user must provide these) ----
# FREERTOS_DIR  : path to FreeRTOS/Source
# HAL_DIR       : path to STM32 HAL Drivers
# CMSIS_DIR     : path to CMSIS Core/Device

set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${FREERTOS_DIR}/include
    ${HAL_DIR}/Inc
    ${CMSIS_DIR}/Include
)

# ---- Static library ----
add_library(uart_dma_framework STATIC ${SOURCES})
target_include_directories(uart_dma_framework PUBLIC ${INCLUDE_DIRS})

# ---- Examples (optional, requires FreeRTOS + HAL + startup) ----
# add_executable(echo_demo example/echo_demo.c)
# target_link_libraries(echo_demo uart_dma_framework ${FREERTOS_LIB} ${HAL_LIB})
