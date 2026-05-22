/**
 * @file    uart_port.h
 * @brief   Unified port interface for DMA-UART driver
 *
 * Each MCU family implements these functions in its own port_uart.c.
 * The upper driver layer only depends on this header, not on HAL specifics.
 */

#ifndef UART_PORT_H
#define UART_PORT_H

#include <stdint.h>
#include <stdbool.h>

/* max supported UART ports */
#define UART_PORT_MAX    3

/* UART port identifiers */
#define UART_PORT1       0
#define UART_PORT2       1
#define UART_PORT3       2

/* callback types */
typedef void (*uart_port_tx_tc_cb_t)(uint8_t port);
typedef void (*uart_port_tx_ht_cb_t)(uint8_t port);
typedef void (*uart_port_rx_tc_cb_t)(uint8_t port);
typedef void (*uart_port_rx_ht_cb_t)(uint8_t port);
typedef void (*uart_port_idle_cb_t)(uint8_t port);
typedef void (*uart_port_error_cb_t)(uint8_t port, uint32_t error);

/* ---- HAL init / deinit ---- */
void uart_port_init(uint8_t port, uint32_t baudrate);
void uart_port_deinit(uint8_t port);

/* ---- DMA operations ---- */
void uart_port_dma_tx_start(uint8_t port, const uint8_t *data, uint32_t len);
void uart_port_dma_rx_start(uint8_t port, uint8_t *buf, uint32_t len);
void uart_port_dma_tx_stop(uint8_t port);
void uart_port_dma_rx_stop(uint8_t port);

/* ---- DMA status ---- */
uint32_t uart_port_tx_dma_remaining(uint8_t port);
uint32_t uart_port_rx_dma_remaining(uint8_t port);

/* ---- GPIO / flow control (optional) ---- */
void uart_port_rts_set(uint8_t port, bool assert);
bool uart_port_cts_read(uint8_t port);

/* ---- ISR callback registration ---- */
void uart_port_set_tx_tc_cb(uint8_t port, uart_port_tx_tc_cb_t cb);
void uart_port_set_tx_ht_cb(uint8_t port, uart_port_tx_ht_cb_t cb);
void uart_port_set_rx_tc_cb(uint8_t port, uart_port_rx_tc_cb_t cb);
void uart_port_set_rx_ht_cb(uint8_t port, uart_port_rx_ht_cb_t cb);
void uart_port_set_idle_cb(uint8_t port, uart_port_idle_cb_t cb);
void uart_port_set_error_cb(uint8_t port, uart_port_error_cb_t cb);

/* ---- HAL IRQ handlers (call from stm32f1xx_it.c) ---- */
void uart_port_irq_handler(uint8_t port);
void uart_port_dma_tx_irq_handler(uint8_t port);
void uart_port_dma_rx_irq_handler(uint8_t port);

/* ---- Helpers ---- */
bool uart_port_dma_tx_busy(uint8_t port);
bool uart_port_dma_rx_busy(uint8_t port);

#endif /* UART_PORT_H */
