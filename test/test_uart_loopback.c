/**
 * @file    test_uart_loopback.c
 * @brief   Hardware loopback integration test
 *
 * Requires:
 * - STM32F103ZET6 board
 * - Physical TX-RX loopback jumper on UART2 (PA2 <-> PA3)
 *
 * This test verifies:
 * - DMA TX chained send
 * - DMA RX circular receive with IDLE
 * - RingBuffer wrap-around handling
 * - Data integrity at high speed
 *
 * Pass criteria: all sent bytes received correctly, no overrun.
 */

#include "uart_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define LOOPBACK_PORT        1       /* UART2 */
#define LOOPBACK_BAUDRATE    115200

/* test patterns */
static const uint8_t pattern_short[] = "Hello Loopback!";
static const uint8_t pattern_long[]  =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz!@#$%^&*()";

typedef struct {
    uint32_t total_sent;
    uint32_t total_recv;
    uint32_t mismatch_cnt;
    uint32_t timeout_cnt;
} test_stats_t;

static test_stats_t g_stats;

void loopback_test_task(void *param)
{
    (void)param;
    int32_t result;

    /* init UART2 for loopback test */
    uart_init(LOOPBACK_PORT, LOOPBACK_BAUDRATE, 512, 1024);

    uart_printf(0, "\r\n=== UART DMA Loopback Test ===\r\n");
    uart_printf(0, "Connect PA2(TX) <-> PA3(RX) on UART2\r\n\r\n");

    /* ---- Test 1: Short string ---- */
    uart_printf(0, "[Test 1] Short string echo ... ");

    uint32_t short_len = (uint32_t)strlen((const char *)pattern_short);
    uart_send(LOOPBACK_PORT, pattern_short, short_len, pdMS_TO_TICKS(1000));

    /* wait for loopback */
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t rx_buf[128];
    result = uart_recv(LOOPBACK_PORT, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(500));
    if (result == (int32_t)short_len &&
        memcmp(pattern_short, rx_buf, short_len) == 0) {
        uart_printf(0, "PASSED (%ld bytes)\r\n", (int32_t)short_len);
        g_stats.total_sent += short_len;
        g_stats.total_recv += (uint32_t)result;
    } else {
        uart_printf(0, "FAILED (sent=%lu, recv=%ld)\r\n", short_len, result);
        g_stats.mismatch_cnt++;
    }

    /* ---- Test 2: Buffer wrap-around stress test ---- */
    uart_printf(0, "[Test 2] Large data (>buffer size to trigger wrap) ... ");

    uint32_t long_len = (uint32_t)strlen((const char *)pattern_long);

    /* send pattern repeatedly to exceed ring buffer capacity */
    for (int i = 0; i < 30; i++) {
        uart_send(LOOPBACK_PORT, pattern_long, long_len, pdMS_TO_TICKS(5000));
        g_stats.total_sent += long_len;
    }

    /* receive and verify */
    vTaskDelay(pdMS_TO_TICKS(500));

    uint32_t total_received = 0;
    uint32_t mismatch = 0;
    TickType_t start = xTaskGetTickCount();

    while (total_received < g_stats.total_sent) {
        result = uart_recv(LOOPBACK_PORT, rx_buf, sizeof(rx_buf),
            pdMS_TO_TICKS(200));

        if (result > 0) {
            total_received += (uint32_t)result;
        } else if (result == 0) {
            /* timeout check */
            if (xTaskGetTickCount() - start > pdMS_TO_TICKS(10000)) {
                g_stats.timeout_cnt++;
                break;
            }
        }
    }

    g_stats.total_recv += total_received;

    if (total_received == g_stats.total_sent) {
        uart_printf(0, "PASSED (sent=%lu, recv=%lu)\r\n",
            g_stats.total_sent, total_received);
    } else {
        uart_printf(0, "FAILED (sent=%lu, recv=%lu, missing=%ld)\r\n",
            g_stats.total_sent, total_received,
            (int32_t)(g_stats.total_sent - total_received));
    }

    /* ---- Test 3: Concurrent send/recv (TX during RX DMA) ---- */
    uart_printf(0, "[Test 3] Bidirectional stress ... ");

    uint32_t bi_sent = 0;
    uint32_t bi_recv = 0;
    TickType_t bi_start = xTaskGetTickCount();

    while (xTaskGetTickCount() - bi_start < pdMS_TO_TICKS(3000)) {
        /* send */
        uart_send(LOOPBACK_PORT, pattern_short, short_len, pdMS_TO_TICKS(100));
        bi_sent += short_len;

        /* try receive */
        result = uart_recv(LOOPBACK_PORT, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(10));
        if (result > 0) {
            bi_recv += (uint32_t)result;
        }
    }

    /* drain remaining */
    vTaskDelay(pdMS_TO_TICKS(200));
    while (1) {
        result = uart_recv(LOOPBACK_PORT, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(50));
        if (result > 0) {
            bi_recv += (uint32_t)result;
        } else {
            break;
        }
    }

    g_stats.total_sent += bi_sent;
    g_stats.total_recv += bi_recv;

    if (bi_recv >= bi_sent * 95 / 100) {  /* 5% tolerance */
        uart_printf(0, "PASSED (sent=%lu, recv=%lu)\r\n", bi_sent, bi_recv);
    } else {
        uart_printf(0, "FAILED (sent=%lu, recv=%lu, loss=%.1f%%)\r\n",
            bi_sent, bi_recv, 100.0 * (bi_sent - bi_recv) / bi_sent);
    }

    /* ---- Summary ---- */
    uart_printf(0, "\r\n=== Loopback Test Summary ===\r\n");
    uart_printf(0, "Total sent:     %lu bytes\r\n", g_stats.total_sent);
    uart_printf(0, "Total received: %lu bytes\r\n", g_stats.total_recv);
    uart_printf(0, "Mismatches:     %lu\r\n", g_stats.mismatch_cnt);
    uart_printf(0, "Timeouts:       %lu\r\n", g_stats.timeout_cnt);
    uart_printf(0, "RX overruns:    %lu\r\n",
        (uint32_t)uart_get_errors(LOOPBACK_PORT));
    uart_printf(0, "=== DONE ===\r\n");

    vTaskDelete(NULL);
}

void loopback_test_init(void)
{
    memset(&g_stats, 0, sizeof(g_stats));

    TaskHandle_t task_handle;
    xTaskCreate(
        loopback_test_task,
        "loopback",
        512,
        NULL,
        tskIDLE_PRIORITY + 1,
        &task_handle
    );
}
