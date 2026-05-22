/**
 * @file    uart_dma.c
 * @brief   DMA-UART driver implementation
 *
 * TX flow:
 *  1. uart_dma_send() → rb_put() into tx_rb
 *  2. If DMA idle, dma_tx_kick() → calculate contiguous segment, start DMA
 *  3. DMA TC ISR → advance tail, check for more data in ring buffer
 *  4. If ring buffer wraps (head < tail), chain 2nd DMA segment
 *  5. When ring buffer empty, DMA stops, send TaskNotify to waiting task
 *
 * RX flow:
 *  1. Init: start DMA circular mode on rx_rb
 *  2. DMA HT/TC ISR → advance head in rx_rb
 *  3. UART IDLE ISR → read NDTR, calculate received bytes, advance head
 *  4. TaskNotify to waiting task that data is available
 */

#include "uart_dma.h"
#include "../ringbuffer/ringbuffer.h"
#include "../port/uart_port.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* ---- driver handle definition ---- */

struct uart_dma_handle {
    uint8_t       port;
    ringbuffer_t  tx_rb;
    ringbuffer_t  rx_rb;

    /* FreeRTOS notification */
    TaskHandle_t  tx_task;       /* task waiting for TX completion */
    TaskHandle_t  rx_task;       /* task waiting for RX data */

    /* for tx chaining: head snapshot when kick started */
    volatile uint32_t  tx_kick_head;  /* head value when dma_tx_kick() was called */
    volatile bool      tx_chain_pending; /* second segment needed */

    /* error tracking */
    volatile uint32_t  rx_overrun_cnt;
    volatile uint32_t  error_flags;
    volatile bool      tx_idle;   /* true if DMA TX is not running */
};

/* ---- forward declarations ---- */

static void dma_tx_kick(uart_dma_handle_t *h);
static void dma_tx_tc_isr(uint8_t port);
static void dma_rx_ht_isr(uint8_t port);
static void dma_rx_tc_isr(uint8_t port);
static void dma_idle_isr(uint8_t port);
static void dma_error_isr(uint8_t port, uint32_t error);

/* ---- storage for per-port driver handles ---- */

static uart_dma_handle_t *g_drv_handles[UART_PORT_MAX];

/* ---- public API ---- */

uart_dma_handle_t *uart_dma_open(uint8_t port, uint32_t baudrate,
    uint8_t *tx_buf, uint32_t tx_buf_size,
    uint8_t *rx_buf, uint32_t rx_buf_size)
{
    if (port >= UART_PORT_MAX) return NULL;

    uart_dma_handle_t *h = (uart_dma_handle_t *)pvPortMalloc(sizeof(uart_dma_handle_t));
    if (!h) return NULL;

    h->port = port;
    rb_init(&h->tx_rb, tx_buf, tx_buf_size);
    rb_init(&h->rx_rb, rx_buf, rx_buf_size);

    h->tx_task          = xTaskGetCurrentTaskHandle();
    h->rx_task          = xTaskGetCurrentTaskHandle();
    h->tx_kick_head     = 0;
    h->tx_chain_pending = false;
    h->rx_overrun_cnt   = 0;
    h->error_flags      = 0;
    h->tx_idle          = true;

    /* register ISR callbacks */
    uart_port_set_tx_tc_cb(port, dma_tx_tc_isr);
    uart_port_set_rx_ht_cb(port, dma_rx_ht_isr);
    uart_port_set_rx_tc_cb(port, dma_rx_tc_isr);
    uart_port_set_idle_cb(port, dma_idle_isr);
    uart_port_set_error_cb(port, dma_error_isr);

    /* init hardware */
    uart_port_init(port, baudrate);

    /* start RX DMA in circular mode on the entire rx buffer */
    uart_port_dma_rx_start(port, h->rx_rb.buffer, h->rx_rb.capacity);

    g_drv_handles[port] = h;
    return h;
}

void uart_dma_close(uart_dma_handle_t *h)
{
    if (!h) return;

    uart_port_dma_tx_stop(h->port);
    uart_port_dma_rx_stop(h->port);
    uart_port_deinit(h->port);

    g_drv_handles[h->port] = NULL;
    vPortFree(h);
}

/* ---- TX ---- */

uint32_t uart_dma_send(uart_dma_handle_t *h, const uint8_t *data, uint32_t len)
{
    if (!h || !data || len == 0) return 0;

    uint32_t written = rb_put(&h->tx_rb, data, len);

    /* if DMA TX is idle, kick it */
    if (h->tx_idle && !rb_is_empty(&h->tx_rb)) {
        dma_tx_kick(h);
    }

    return written;
}

bool uart_dma_tx_flush(uart_dma_handle_t *h, uint32_t timeout)
{
    if (!h) return true;

    TickType_t start = xTaskGetTickCount();

    while (!rb_is_empty(&h->tx_rb) || !h->tx_idle) {
        /* wait for TX complete notification */
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));
        (void)notified;

        if (timeout != portMAX_DELAY) {
            TickType_t elapsed = xTaskGetTickCount() - start;
            if (elapsed >= timeout) {
                return false;
            }
        }
    }
    return true;
}

/* ---- RX ---- */

uint32_t uart_dma_recv(uart_dma_handle_t *h, uint8_t *buf, uint32_t len)
{
    if (!h || !buf || len == 0) return 0;
    return rb_get(&h->rx_rb, buf, len);
}

uint32_t uart_dma_rx_available(uart_dma_handle_t *h)
{
    if (!h) return 0;
    return rb_available(&h->rx_rb);
}

uint32_t uart_dma_tx_free_space(uart_dma_handle_t *h)
{
    if (!h) return 0;
    return rb_free_space(&h->tx_rb);
}

void uart_dma_rx_flush(uart_dma_handle_t *h)
{
    if (!h) return;
    rb_flush(&h->rx_rb);
}

uint32_t uart_dma_get_errors(uart_dma_handle_t *h)
{
    if (!h) return 0;
    return h->error_flags;
}

bool uart_dma_tx_busy(uart_dma_handle_t *h)
{
    if (!h) return false;
    return !h->tx_idle;
}

/* ================================================================
 * Internal: TX DMA engine
 * ================================================================ */

/**
 * @brief Kick off DMA TX from ring buffer
 *
 * Algorithm:
 * 1. Snapshot head to prevent new writes from confusing segment boundary
 * 2. If head == tail: nothing to send, done
 * 3. If tail < head: single contiguous segment [tail, head)
 * 4. If tail > head: data wrapped around
 *    a. 1st DMA: [tail, capacity)
 *    b. Set chain_pending flag for 2nd segment [0, head) in TC ISR
 */
static void dma_tx_kick(uart_dma_handle_t *h)
{
    uint32_t head = rb_get_head(&h->tx_rb);
    uint32_t tail = rb_get_tail(&h->tx_rb);

    if (head == tail) {
        /* nothing to send */
        h->tx_idle = true;
        return;
    }

    h->tx_kick_head = head;
    h->tx_idle = false;

    uint32_t tail_idx = tail & h->tx_rb.mask;
    uint32_t head_idx = head & h->tx_rb.mask;

    if (tail < head) {
        /* ---- no wrap: single contiguous segment ---- */
        uint32_t seg_len = head - tail;
        h->tx_chain_pending = false;
        uart_port_dma_tx_start(h->port, &h->tx_rb.buffer[tail_idx], seg_len);
    } else {
        /* ---- wrap: data = [tail, capacity) + [0, head) ---- */
        uint32_t seg1_len = h->tx_rb.capacity - tail_idx;
        h->tx_chain_pending = (head_idx > 0); /* 2nd segment exists */
        uart_port_dma_tx_start(h->port, &h->tx_rb.buffer[tail_idx], seg1_len);
    }
}

/* ================================================================
 * ISR callbacks (called from port layer, in ISR context)
 * ================================================================ */

/**
 * @brief TX DMA Transfer Complete ISR
 *
 * Called when one DMA segment finishes.
 * Checks if there's a chained 2nd segment (wrap case) or more data.
 */
static void dma_tx_tc_isr(uint8_t port)
{
    uart_dma_handle_t *h = g_drv_handles[port];
    if (!h) return;

    if (h->tx_chain_pending) {
        /* ---- 2nd segment: [0, tx_kick_head) ---- */
        uint32_t snap_head = h->tx_kick_head;
        h->tx_chain_pending = false;

        /* advance tail past the 1st segment */
        uint32_t seg1_end = h->tx_rb.capacity;
        h->tx_rb.tail = (h->tx_rb.tail & ~h->tx_rb.mask) + seg1_end;
        /* clamp: don't overtake head */
        if (h->tx_rb.tail > snap_head) {
            h->tx_rb.tail = snap_head;
        }

        uint32_t seg2_len = snap_head & h->tx_rb.mask;
        if (seg2_len > 0) {
            uart_port_dma_tx_start(h->port, &h->tx_rb.buffer[0], seg2_len);
            /* chain_pending remains false; next TC goes to check_more below */
        } else {
            goto check_more;
        }
    } else {
        /* ---- single segment completed ---- */
        uint32_t snap_head = h->tx_kick_head;
        h->tx_rb.tail = snap_head;

check_more:
        /* check if new data arrived while DMA was running */
        uint32_t cur_head = rb_get_head(&h->tx_rb);
        uint32_t cur_tail = rb_get_tail(&h->tx_rb);

        if (cur_head != cur_tail) {
            /* more data: re-kick */
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            dma_tx_kick(h);
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        } else {
            /* all data sent */
            h->tx_idle = true;

            /* notify waiting tx task */
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            if (h->tx_task) {
                vTaskNotifyGiveFromISR(h->tx_task, &xHigherPriorityTaskWoken);
            }
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    }
}

/**
 * @brief RX DMA Half-Transfer ISR
 *
 * DMA circular mode: HT fires when CNDTR reaches half the buffer size.
 * We calculate bytes received: half the buffer minus what's left.
 */
static void dma_rx_ht_isr(uint8_t port)
{
    uart_dma_handle_t *h = g_drv_handles[port];
    if (!h) return;

    /* bytes already transferred = buffer_size - ndtr */
    uint32_t ndtr      = uart_port_rx_dma_remaining(port);
    uint32_t capacity  = h->rx_rb.capacity;
    uint32_t received  = capacity - ndtr;

    /* head should already be at or near half; set it */
    uint32_t old_head  = rb_get_head(&h->rx_rb);
    uint32_t new_head  = (old_head & ~h->rx_rb.mask) + received;

    /*
     * On circular DMA, the total received count may exceed capacity
     * (DMA wraps around). We normalize to monotonic counter.
     */
    if (new_head <= old_head) {
        /* DMA wrapped past old_head; this means an overrun occurred */
        new_head = old_head + (received % capacity);
        if (new_head <= old_head) {
            new_head = old_head + capacity;
        }
    }

    /* Cap advance to not exceed old_head + capacity */
    if (new_head > old_head + capacity) {
        new_head = old_head + capacity;
    }

    uint32_t advance = new_head - old_head;
    if (advance > 0) {
        rb_advance_head(&h->rx_rb, advance);
    }
}

/**
 * @brief RX DMA Transfer Complete ISR
 */
static void dma_rx_tc_isr(uint8_t port)
{
    uart_dma_handle_t *h = g_drv_handles[port];
    if (!h) return;

    uint32_t ndtr      = uart_port_rx_dma_remaining(port);
    uint32_t capacity  = h->rx_rb.capacity;
    uint32_t received  = capacity - ndtr;

    uint32_t old_head  = rb_get_head(&h->rx_rb);
    uint32_t new_head  = (old_head & ~h->rx_rb.mask) + received;

    if (new_head <= old_head) {
        new_head = old_head + capacity;
    }
    if (new_head > old_head + capacity) {
        new_head = old_head + capacity;
    }

    uint32_t advance = new_head - old_head;
    if (advance > 0) {
        rb_advance_head(&h->rx_rb, advance);
    }
}

/**
 * @brief UART IDLE interrupt
 *
 * Fires when RXD line stays idle for 1 byte time after last byte.
 * Read NDTR to calculate exact bytes received since last update.
 * Critical: read SR+DR to clear IDLE before reading NDTR (avoids race).
 */
static void dma_idle_isr(uint8_t port)
{
    uart_dma_handle_t *h = g_drv_handles[port];
    if (!h) return;

    /* IDLE flag already cleared by port layer (SR+DR read).
     * Now read NDTR to determine how many bytes DMA transferred. */
    uint32_t ndtr      = uart_port_rx_dma_remaining(port);
    uint32_t capacity  = h->rx_rb.capacity;
    uint32_t received  = capacity - ndtr;

    uint32_t old_head  = rb_get_head(&h->rx_rb);
    uint32_t modulo    = old_head % capacity;
    uint32_t base      = old_head - modulo;

    /* received is DMA's absolute position in the circular buffer */
    uint32_t new_head;
    if (received > modulo) {
        new_head = base + received;
    } else if (received < modulo) {
        /* DMA wrapped: modulo > received means DMA is now behind old_head */
        new_head = base + capacity + received;
    } else {
        /* received == modulo: no new data */
        return;
    }

    if (new_head > old_head + capacity) {
        new_head = old_head + capacity;
    }

    uint32_t advance = new_head - old_head;
    if (advance > 0) {
        rb_advance_head(&h->rx_rb, advance);

        /* notify waiting rx task */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (h->rx_task) {
            vTaskNotifyGiveFromISR(h->rx_task, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

/**
 * @brief UART error ISR callback
 */
static void dma_error_isr(uint8_t port, uint32_t error)
{
    uart_dma_handle_t *h = g_drv_handles[port];
    if (!h) return;

    h->error_flags |= error;

    if (error & USART_SR_ORE) {
        h->rx_overrun_cnt++;
    }
}
