/**
 * @file    uart_dma.h
 * @brief   DMA-UART driver layer
 *
 * Bridges the ring buffer and the HAL port layer:
 * - TX: consumer of tx_rb, feeds DMA in chained segments (handles wrap-around)
 * - RX: producer for rx_rb, driven by DMA circular + IDLE interrupts
 * - Uses FreeRTOS task notification or semaphore for task wake-up
 */

#ifndef UART_DMA_H
#define UART_DMA_H

#include <stdint.h>
#include <stdbool.h>

/* driver handle */
typedef struct uart_dma_handle uart_dma_handle_t;

/**
 * @brief Open a DMA-UART driver instance
 * @param port       UART port index (0-based)
 * @param baudrate   baud rate (e.g. 115200)
 * @param tx_buf     tx ring buffer storage array
 * @param tx_buf_size tx buffer capacity (must be power of 2)
 * @param rx_buf     rx ring buffer storage array
 * @param rx_buf_size rx buffer capacity (must be power of 2)
 * @return driver handle, NULL on failure
 */
uart_dma_handle_t *uart_dma_open(uint8_t port, uint32_t baudrate,
    uint8_t *tx_buf, uint32_t tx_buf_size,
    uint8_t *rx_buf, uint32_t rx_buf_size);

/**
 * @brief Close and deinit driver instance
 */
void uart_dma_close(uart_dma_handle_t *h);

/**
 * @brief Send data (non-blocking). Writes to tx ring buffer and starts DMA if idle.
 * @param h    driver handle
 * @param data source data
 * @param len  bytes to send
 * @return actual bytes enqueued (may be less than len if buffer full)
 */
uint32_t uart_dma_send(uart_dma_handle_t *h, const uint8_t *data, uint32_t len);

/**
 * @brief Receive data (non-blocking). Reads from rx ring buffer.
 * @param h   driver handle
 * @param buf destination buffer
 * @param len max bytes to read
 * @return actual bytes read
 */
uint32_t uart_dma_recv(uart_dma_handle_t *h, uint8_t *buf, uint32_t len);

/**
 * @brief Get available RX bytes
 */
uint32_t uart_dma_rx_available(uart_dma_handle_t *h);

/**
 * @brief Get free TX space
 */
uint32_t uart_dma_tx_free_space(uart_dma_handle_t *h);

/**
 * @brief Flush TX: wait until all data sent or timeout
 * @param h       driver handle
 * @param timeout FreeRTOS tick timeout (portMAX_DELAY for infinite)
 * @return true if fully flushed, false on timeout
 */
bool uart_dma_tx_flush(uart_dma_handle_t *h, uint32_t timeout);

/**
 * @brief Flush RX: discard all received data
 */
void uart_dma_rx_flush(uart_dma_handle_t *h);

/**
 * @brief Get last error flags
 */
uint32_t uart_dma_get_errors(uart_dma_handle_t *h);

/**
 * @brief Check if port is currently sending
 */
bool uart_dma_tx_busy(uart_dma_handle_t *h);

#endif /* UART_DMA_H */
