/**
 * @file    uart_api.c
 * @brief   High-level UART API implementation
 *
 * Thread safety per port: each port has its own FreeRTOS mutex,
 * so multiple tasks can safely call uart_send/uart_printf on the same port
 * without data interleaving. Different ports are independently locked.
 */

#include "uart_api.h"
#include "../driver/uart_dma.h"
#include "../config/uart_config.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- per-port context ---- */

typedef struct {
    uart_dma_handle_t  *drv;
    SemaphoreHandle_t   tx_mutex;
    TaskHandle_t        rx_task;        /* task waiting in uart_yield */
    uart_rx_cb_t        rx_cb;
    void               *rx_cb_arg;
    uint8_t            *tx_buf_storage;
    uint8_t            *rx_buf_storage;
    uint32_t            tx_buf_size;
    uint32_t            rx_buf_size;
    bool                initialized;
} uart_ctx_t;

static uart_ctx_t g_ctx[UART_PORT_MAX];

/* ---- internal helpers ---- */

static uart_ctx_t *ctx_get(uint8_t port)
{
    if (port >= UART_PORT_MAX) return NULL;
    return &g_ctx[port];
}

/* ---- public API ---- */

void uart_init(uint8_t port, uint32_t baudrate,
               uint32_t tx_buf_size, uint32_t rx_buf_size)
{
    if (port >= UART_PORT_MAX) return;

    uart_ctx_t *ctx = &g_ctx[port];

    /* free previous instance if re-initializing */
    if (ctx->initialized) {
        uart_deinit(port);
    }

    memset(ctx, 0, sizeof(*ctx));

    /* allocate ring buffer storage */
    ctx->tx_buf_storage = (uint8_t *)pvPortMalloc(tx_buf_size);
    ctx->rx_buf_storage = (uint8_t *)pvPortMalloc(rx_buf_size);
    ctx->tx_buf_size    = tx_buf_size;
    ctx->rx_buf_size    = rx_buf_size;

    if (!ctx->tx_buf_storage || !ctx->rx_buf_storage) {
        /* allocation failure: free and bail */
        if (ctx->tx_buf_storage) { vPortFree(ctx->tx_buf_storage); }
        if (ctx->rx_buf_storage) { vPortFree(ctx->rx_buf_storage); }
        return;
    }

    /* create mutex for TX path */
    ctx->tx_mutex = xSemaphoreCreateMutex();
    if (!ctx->tx_mutex) {
        vPortFree(ctx->tx_buf_storage);
        vPortFree(ctx->rx_buf_storage);
        return;
    }

    /* open driver */
    ctx->drv = uart_dma_open(port, baudrate,
        ctx->tx_buf_storage, tx_buf_size,
        ctx->rx_buf_storage, rx_buf_size);

    if (!ctx->drv) {
        vSemaphoreDelete(ctx->tx_mutex);
        vPortFree(ctx->tx_buf_storage);
        vPortFree(ctx->rx_buf_storage);
        return;
    }

    ctx->initialized = true;
}

void uart_deinit(uint8_t port)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return;

    if (ctx->drv) {
        uart_dma_close(ctx->drv);
        ctx->drv = NULL;
    }
    if (ctx->tx_mutex) {
        vSemaphoreDelete(ctx->tx_mutex);
        ctx->tx_mutex = NULL;
    }
    if (ctx->tx_buf_storage) {
        vPortFree(ctx->tx_buf_storage);
        ctx->tx_buf_storage = NULL;
    }
    if (ctx->rx_buf_storage) {
        vPortFree(ctx->rx_buf_storage);
        ctx->rx_buf_storage = NULL;
    }

    ctx->initialized = false;
}

/* ---- blocking send ---- */

int32_t uart_send(uint8_t port, const uint8_t *data, uint32_t len, uint32_t timeout)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized || !data || len == 0) return -1;

    /* acquire TX mutex */
    if (xSemaphoreTake(ctx->tx_mutex, timeout) != pdTRUE) {
        return -1;
    }

    uint32_t total_sent = 0;
    TickType_t start = xTaskGetTickCount();

    while (total_sent < len) {
        /* space check */
        uint32_t free_space = uart_dma_tx_free_space(ctx->drv);

        if (free_space == 0) {
            /* no space: check timeout */
            if (timeout != portMAX_DELAY) {
                TickType_t elapsed = xTaskGetTickCount() - start;
                if (elapsed >= timeout) {
                    break;
                }
            }
            /* yield and retry */
            xSemaphoreGive(ctx->tx_mutex);
            vTaskDelay(pdMS_TO_TICKS(1));
            if (xSemaphoreTake(ctx->tx_mutex, timeout) != pdTRUE) {
                return (total_sent > 0) ? (int32_t)total_sent : -1;
            }
            continue;
        }

        uint32_t remaining = len - total_sent;
        uint32_t chunk = (remaining < free_space) ? remaining : free_space;

        uint32_t written = uart_dma_send(ctx->drv, data + total_sent, chunk);
        total_sent += written;

        if (written == 0) {
            break; /* should not happen if free_space > 0 */
        }
    }

    xSemaphoreGive(ctx->tx_mutex);
    return (int32_t)total_sent;
}

/* ---- blocking recv ---- */

int32_t uart_recv(uint8_t port, uint8_t *buf, uint32_t len, uint32_t timeout)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized || !buf || len == 0) return -1;

    /* Store our task handle for ISR notification */
    ctx->drv->rx_task = xTaskGetCurrentTaskHandle();

    TickType_t start = xTaskGetTickCount();

    while (1) {
        uint32_t avail = uart_dma_rx_available(ctx->drv);
        if (avail > 0) {
            uint32_t read_len = (len < avail) ? len : avail;
            return (int32_t)uart_dma_recv(ctx->drv, buf, read_len);
        }

        /* nothing available: wait for notification */
        uint32_t wait_ticks = pdMS_TO_TICKS(10);
        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            if (elapsed >= timeout) {
                return 0; /* timeout with 0 bytes */
            }
            TickType_t remaining = timeout - elapsed;
            if (wait_ticks > remaining) {
                wait_ticks = remaining;
            }
        }

        /* Wait for ISR notification (IDLE or HT/TC) */
        ulTaskNotifyTake(pdTRUE, wait_ticks);
    }
}

/* ---- non-blocking ---- */

int32_t uart_send_async(uint8_t port, const uint8_t *data, uint32_t len)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized || !data || len == 0) return -1;

    /* try non-blocking mutex acquisition */
    if (xSemaphoreTake(ctx->tx_mutex, 0) != pdTRUE) {
        return -1; /* another task is sending */
    }

    uint32_t free_space = uart_dma_tx_free_space(ctx->drv);
    if (free_space == 0) {
        xSemaphoreGive(ctx->tx_mutex);
        return -1;
    }

    uint32_t chunk = (len < free_space) ? len : free_space;
    uint32_t written = uart_dma_send(ctx->drv, data, chunk);

    xSemaphoreGive(ctx->tx_mutex);
    return (int32_t)written;
}

int32_t uart_recv_async(uint8_t port, uint8_t *buf, uint32_t len)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized || !buf || len == 0) return -1;

    return (int32_t)uart_dma_recv(ctx->drv, buf, len);
}

/* ---- flush ---- */

bool uart_tx_flush(uint8_t port, uint32_t timeout)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return true;

    /* acquire mutex to ensure no ongoing writes */
    if (xSemaphoreTake(ctx->tx_mutex, timeout) != pdTRUE) {
        return false;
    }

    bool result = uart_dma_tx_flush(ctx->drv, timeout);

    xSemaphoreGive(ctx->tx_mutex);
    return result;
}

void uart_rx_flush(uint8_t port)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return;
    uart_dma_rx_flush(ctx->drv);
}

/* ---- printf (formatted output) ---- */

#if UART_PRINTF_ENABLE

int32_t uart_printf(uint8_t port, const char *fmt, ...)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized || !fmt) return -1;

    static char printf_buf[UART_PRINTF_BUF_SIZE];

    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(printf_buf, sizeof(printf_buf), fmt, args);
    va_end(args);

    if (len <= 0) return -1;
    if ((uint32_t)len >= sizeof(printf_buf)) {
        len = (int)(sizeof(printf_buf) - 1);
    }

    return uart_send(port, (const uint8_t *)printf_buf, (uint32_t)len, portMAX_DELAY);
}

#endif /* UART_PRINTF_ENABLE */

/* ---- RX callback / yield ---- */

void uart_set_rx_callback(uint8_t port, uart_rx_cb_t cb, void *arg)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx) return;
    ctx->rx_cb     = cb;
    ctx->rx_cb_arg = arg;
}

void uart_yield(uint8_t port, uint32_t timeout)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return;

    ctx->drv->rx_task = xTaskGetCurrentTaskHandle();
    ctx->rx_task      = xTaskGetCurrentTaskHandle();

    /*
     * Wait for RX notification, then invoke callback.
     * This loop drains all available frames within the timeout window.
     */
    TickType_t start = xTaskGetTickCount();

    while (1) {
        uint32_t avail = uart_dma_rx_available(ctx->drv);

        if (avail > 0 && ctx->rx_cb) {
            /*
             * Read all available data as one "frame" (delimited by IDLE).
             * Use a local buffer on the task stack.
             */
            uint8_t frame_buf[256];
            uint32_t read_len = (avail < sizeof(frame_buf)) ? avail : sizeof(frame_buf);
            uint32_t n = uart_dma_recv(ctx->drv, frame_buf, read_len);
            if (n > 0) {
                ctx->rx_cb(port, frame_buf, n, ctx->rx_cb_arg);
            }
        }

        /* wait for next IDLE event */
        uint32_t wait_ticks = pdMS_TO_TICKS(100);
        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            if (elapsed >= timeout) {
                break;
            }
            TickType_t remaining = timeout - elapsed;
            if (wait_ticks > remaining) {
                wait_ticks = remaining;
            }
        }

        ulTaskNotifyTake(pdTRUE, wait_ticks);
    }
}

/* ---- status ---- */

uint32_t uart_rx_available(uint8_t port)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return 0;
    return uart_dma_rx_available(ctx->drv);
}

uint32_t uart_tx_free_space(uint8_t port)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return 0;
    return uart_dma_tx_free_space(ctx->drv);
}

uint32_t uart_get_errors(uint8_t port)
{
    uart_ctx_t *ctx = ctx_get(port);
    if (!ctx || !ctx->initialized) return 0;
    return uart_dma_get_errors(ctx->drv);
}
