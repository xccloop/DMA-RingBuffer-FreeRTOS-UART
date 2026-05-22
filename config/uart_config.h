/**
 * @file    uart_config.h
 * @brief   Global configuration for DMA-UART framework
 *
 * All tunable parameters centralized here.
 * Modify buffer sizes, DMA modes, and feature flags to match your application.
 */

#ifndef UART_CONFIG_H
#define UART_CONFIG_H

/* ================================================================
 * Port count
 * ================================================================ */
#define UART_PORT_COUNT             3

/* ================================================================
 * Buffer sizes (MUST be power of 2: 128, 256, 512, 1024, 2048, ...)
 * Using powers of 2 enables bitmask modulo (x & (capacity-1)),
 * avoiding expensive integer division.
 * ================================================================ */

/* UART1: typically console/debug output (high throughput) */
#define UART1_TX_BUF_SIZE           512
#define UART1_RX_BUF_SIZE           1024

/* UART2: typically peripheral communication (moderate throughput) */
#define UART2_TX_BUF_SIZE           256
#define UART2_RX_BUF_SIZE           512

/* UART3: typically sensor/auxiliary port */
#define UART3_TX_BUF_SIZE           256
#define UART3_RX_BUF_SIZE           512

/* ================================================================
 * DMA mode
 * ================================================================ */
#define UART_USE_DMA_TX             1
#define UART_USE_DMA_RX             1

/* ================================================================
 * IDLE timeout detection (microseconds)
 * Used to detect end-of-frame: if RXD line stays idle for this
 * duration, the UART IDLE interrupt fires.
 * 1000us is 1 byte time at ~8000 bps — safe default.
 * ================================================================ */
#define UART_IDLE_TIMEOUT_US        1000

/* ================================================================
 * ISR notification method
 * 1 = FreeRTOS TaskNotify (faster, less RAM)
 * 0 = Binary Semaphore (compatible with older FreeRTOS versions)
 * ================================================================ */
#define UART_USE_TASK_NOTIFY        1

/* ================================================================
 * Hardware flow control (RTS/CTS)
 * 0 = disabled, 1 = enabled
 * ================================================================ */
#define UART_USE_RTS_CTS            0
#define UART_FLOW_CTL_THRESHOLD     64

/* ================================================================
 * RX overflow policy
 * 0 = discard oldest data (keep newest) — default for streaming
 * 1 = keep oldest data and report overflow to application
 * ================================================================ */
#define UART_RX_OVERFLOW_POLICY     0

/* ================================================================
 * uart_printf support
 * Requires vsnprintf from standard library.
 * Disable to save flash (~2KB).
 * ================================================================ */
#define UART_PRINTF_ENABLE          1
#define UART_PRINTF_BUF_SIZE        256

/* ================================================================
 * Task priorities (adjust per application)
 * ================================================================ */
#define UART_ISR_PRIO               6
#define UART_DMA_ISR_PRIO           7

/* ================================================================
 * Performance tuning
 * ================================================================ */

/* TX retry interval when buffer is full (ms) */
#define UART_TX_RETRY_MS            1

/* RX poll interval in uart_recv (ms) */
#define UART_RX_POLL_MS             10

#endif /* UART_CONFIG_H */
