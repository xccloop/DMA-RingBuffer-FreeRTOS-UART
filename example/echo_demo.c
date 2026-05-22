/**
 * @file    echo_demo.c
 * @brief   UART echo demo using DMA + RingBuffer + FreeRTOS
 *
 * Demonstrates:
 * - uart_init configuration
 * - uart_send / uart_recv blocking API
 * - Simple echo with timeout
 *
 * Expected behavior:
 * - Connect UART1 to a terminal (115200 8N1)
 * - Type characters, they are echoed back
 */

#include "uart_api.h"
#include "FreeRTOS.h"
#include "task.h"

/* task stack size (words) */
#define ECHO_TASK_STACK_SIZE    256

void echo_task(void *param)
{
    uint8_t buf[128];

    /* Init UART1: 115200 baud, 512 TX buffer, 1024 RX buffer */
    uart_init(0, 115200, 512, 1024);

    uart_printf(0, "\r\n=== DMA RingBuffer UART Echo Demo ===\r\n");
    uart_printf(0, "UART1 @ 115200 bps, 8N1\r\n");
    uart_printf(0, "Type something and press Enter...\r\n\r\n");

    while (1) {
        /* Block waiting for data (1 second timeout) */
        int32_t n = uart_recv(0, buf, sizeof(buf), pdMS_TO_TICKS(1000));
        if (n > 0) {
            /* Echo received data back */
            uart_send(0, buf, (uint32_t)n, pdMS_TO_TICKS(100));
        } else {
            /* No data, yield to other tasks */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* Optional: create the echo task from main() */
void echo_demo_init(void)
{
    TaskHandle_t task_handle;
    xTaskCreate(
        echo_task,
        "echo",
        ECHO_TASK_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 2,
        &task_handle
    );
}
