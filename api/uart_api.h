/**
 * @file    uart_api.h
 * @brief   High-level UART API for application tasks
 *
 * Features:
 * - Blocking send/recv with FreeRTOS timeout
 * - Non-blocking async send/recv
 * - Mutex-protected multi-task concurrent access per port
 * - uart_printf formatted output
 * - Frame-based receive callback (driven by UART IDLE)
 */

#ifndef UART_API_H
#define UART_API_H

#include <stdint.h>
#include <stdbool.h>

/* ---- initialization ---- */

/**
 * @brief Initialize a UART port
 * @param port         port index (0 = UART1, 1 = UART2, 2 = UART3)
 * @param baudrate     baud rate (e.g. 115200)
 * @param tx_buf_size  TX ring buffer capacity (power of 2, e.g. 512)
 * @param rx_buf_size  RX ring buffer capacity (power of 2, e.g. 1024)
 */
void uart_init(uint8_t port, uint32_t baudrate,
               uint32_t tx_buf_size, uint32_t rx_buf_size);

void uart_deinit(uint8_t port);

/* ---- blocking API (with timeout) ---- */

/**
 * @brief Send data, block until all data enqueued or timeout
 * @param port    port index
 * @param data    data to send
 * @param len     bytes to send
 * @param timeout FreeRTOS tick timeout
 * @return bytes sent, or -1 on error
 */
int32_t uart_send(uint8_t port, const uint8_t *data, uint32_t len, uint32_t timeout);

/**
 * @brief Receive data, block until data available or timeout
 * @param port    port index
 * @param buf     destination buffer
 * @param len     max bytes to receive
 * @param timeout FreeRTOS tick timeout
 * @return bytes received, or -1 on error
 */
int32_t uart_recv(uint8_t port, uint8_t *buf, uint32_t len, uint32_t timeout);

/**
 * @brief Wait until all TX data is sent
 * @param port    port index
 * @param timeout FreeRTOS tick timeout
 * @return true if flushed, false on timeout
 */
bool uart_tx_flush(uint8_t port, uint32_t timeout);

/* ---- non-blocking API ---- */

/**
 * @brief Send data without blocking. Returns immediately.
 * @return bytes enqueued, or -1 if buffer full
 */
int32_t uart_send_async(uint8_t port, const uint8_t *data, uint32_t len);

/**
 * @brief Receive available data without blocking.
 * @return bytes read (may be 0 if nothing available)
 */
int32_t uart_recv_async(uint8_t port, uint8_t *buf, uint32_t len);

/* ---- formatted output ---- */

#if UART_PRINTF_ENABLE
/**
 * @brief Formatted printf to UART
 * @param port port index
 * @param fmt  printf format string
 * @param ...  format args
 * @return bytes sent, or -1 on error
 */
int32_t uart_printf(uint8_t port, const char *fmt, ...);
#endif

/* ---- RX flush ---- */

void uart_rx_flush(uint8_t port);

/* ---- RX callback (frame-based, driven by IDLE) ---- */

typedef void (*uart_rx_cb_t)(uint8_t port, const uint8_t *data, uint32_t len, void *arg);

/**
 * @brief Register an RX callback invoked on UART IDLE (end of frame)
 * @param port port index
 * @param cb   callback function
 * @param arg  user argument passed to callback
 */
void uart_set_rx_callback(uint8_t port, uart_rx_cb_t cb, void *arg);

/**
 * @brief Block and wait for RX events, invoking registered callbacks
 * @param port    port index
 * @param timeout FreeRTOS tick timeout
 */
void uart_yield(uint8_t port, uint32_t timeout);

/* ---- status ---- */

uint32_t uart_rx_available(uint8_t port);
uint32_t uart_tx_free_space(uint8_t port);
uint32_t uart_get_errors(uint8_t port);

#endif /* UART_API_H */
