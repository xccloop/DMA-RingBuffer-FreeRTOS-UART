/**
 * @file    ringbuffer.h
 * @brief   SPSC lock-free ring buffer for DMA UART framework
 *
 * Single Producer, Single Consumer design:
 * - Producer (DMA ISR for RX / User Task for TX) modifies only head
 * - Consumer (User Task for RX / DMA ISR for TX) modifies only tail
 * - No mutex required; volatile ensures compiler doesn't reorder
 * - Capacity must be power of 2 for bitmask modulo
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    volatile uint32_t head;     /* write index (producer only) */
    volatile uint32_t tail;     /* read index  (consumer only) */
    uint8_t          *buffer;   /* data storage */
    uint32_t          mask;     /* capacity - 1 (capacity must be power of 2) */
    uint32_t          capacity; /* total buffer size */
} ringbuffer_t;

/**
 * @brief Initialize ring buffer
 * @param rb       ring buffer handle
 * @param buf      pre-allocated storage array
 * @param capacity must be power of 2 (e.g. 256, 512, 1024)
 */
void rb_init(ringbuffer_t *rb, uint8_t *buf, uint32_t capacity);

/**
 * @brief Write data into ring buffer
 * @param rb   ring buffer handle
 * @param data source data pointer
 * @param len  number of bytes to write
 * @return     actual bytes written (may be less than len if buffer full)
 */
uint32_t rb_put(ringbuffer_t *rb, const uint8_t *data, uint32_t len);

/**
 * @brief Read data from ring buffer (consumes data)
 * @param rb  ring buffer handle
 * @param dst destination buffer
 * @param len max bytes to read
 * @return    actual bytes read
 */
uint32_t rb_get(ringbuffer_t *rb, uint8_t *dst, uint32_t len);

/**
 * @brief Peek data without consuming
 * @param rb  ring buffer handle (const, does not modify tail)
 * @param dst destination buffer
 * @param len max bytes to peek
 * @return    actual bytes peeked
 */
uint32_t rb_peek(const ringbuffer_t *rb, uint8_t *dst, uint32_t len);

/**
 * @brief Discard n bytes from the read side (advances tail)
 * @param rb  ring buffer handle
 * @param len bytes to discard
 */
void rb_discard(ringbuffer_t *rb, uint32_t len);

/**
 * @brief Get available readable bytes
 */
uint32_t rb_available(const ringbuffer_t *rb);

/**
 * @brief Get free writable space
 */
uint32_t rb_free_space(const ringbuffer_t *rb);

/**
 * @brief Check if ring buffer is empty
 */
bool rb_is_empty(const ringbuffer_t *rb);

/**
 * @brief Check if ring buffer is full
 */
bool rb_is_full(const ringbuffer_t *rb);

/**
 * @brief Flush ring buffer (reset head and tail to 0)
 */
void rb_flush(ringbuffer_t *rb);

/**
 * @brief Get maximum contiguous read length from current tail
 * Useful for DMA: data may wrap around at buffer end
 * @param rb ring buffer handle
 * @return   bytes available contiguously from tail
 */
uint32_t rb_contig_read_len(const ringbuffer_t *rb);

/**
 * @brief Get maximum contiguous write length from current head
 * @param rb ring buffer handle
 * @return   free bytes available contiguously from head
 */
uint32_t rb_contig_write_len(const ringbuffer_t *rb);

/**
 * @brief Advance head pointer (called by DMA ISR after receiving data)
 * @param rb  ring buffer handle
 * @param len bytes received by DMA
 */
void rb_advance_head(ringbuffer_t *rb, uint32_t len);

/**
 * @brief Advance tail pointer (called by DMA ISR after transmitting data)
 * @param rb  ring buffer handle
 * @param len bytes transmitted by DMA
 */
void rb_advance_tail(ringbuffer_t *rb, uint32_t len);

/**
 * @brief Snapshot the current head value (for DMA TX segment planning)
 * @return current head value
 */
static inline uint32_t rb_get_head(const ringbuffer_t *rb)
{
    return rb->head;
}

/**
 * @brief Get the current tail value
 */
static inline uint32_t rb_get_tail(const ringbuffer_t *rb)
{
    return rb->tail;
}

#endif /* RINGBUFFER_H */
