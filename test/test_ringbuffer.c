/**
 * @file    test_ringbuffer.c
 * @brief   Unit tests for ring buffer (host-compilable, no hardware needed)
 *
 * Compile with: gcc -o test_rb test_ringbuffer.c ../ringbuffer/ringbuffer.c -I..
 * Run: ./test_rb
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../ringbuffer/ringbuffer.h"

#define TEST_BUF_SIZE     64
#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        failures++; } else { passed++; } \
} while(0)

static uint8_t buf[TEST_BUF_SIZE];
static ringbuffer_t rb;
static int passed = 0;
static int failures = 0;

static void test_init(void)
{
    printf("Test: rb_init ... ");
    int local_fail = failures;

    /* valid init */
    rb_init(&rb, buf, TEST_BUF_SIZE);
    TEST_ASSERT(rb_is_empty(&rb), "should be empty after init");
    TEST_ASSERT(!rb_is_full(&rb), "should not be full after init");
    TEST_ASSERT(rb_available(&rb) == 0, "available should be 0");
    TEST_ASSERT(rb_free_space(&rb) == TEST_BUF_SIZE, "free space should equal capacity");

    /* non-power-of-2 should be rejected */
    ringbuffer_t rb2;
    uint8_t bad_buf[100];
    rb_init(&rb2, bad_buf, 100);
    TEST_ASSERT(rb2.buffer == NULL, "non-power-of-2 capacity should be rejected");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_put_get(void)
{
    printf("Test: rb_put / rb_get ... ");
    int local_fail = failures;

    rb_flush(&rb);

    uint8_t data[] = "Hello, DMA RingBuffer!";
    uint32_t len   = (uint32_t)strlen((char *)data);

    /* put */
    uint32_t written = rb_put(&rb, data, len);
    TEST_ASSERT(written == len, "all bytes should be written");
    TEST_ASSERT(rb_available(&rb) == len, "available should match written");
    TEST_ASSERT(!rb_is_empty(&rb), "should not be empty after put");

    /* get */
    uint8_t readback[64];
    uint32_t read = rb_get(&rb, readback, sizeof(readback));
    TEST_ASSERT(read == len, "read should return all bytes");
    TEST_ASSERT(memcmp(data, readback, len) == 0, "read data should match written");
    TEST_ASSERT(rb_is_empty(&rb), "should be empty after get");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_wrap_around(void)
{
    printf("Test: wrap-around ... ");
    int local_fail = failures;

    rb_flush(&rb);

    /* advance head and tail near the end */
    uint8_t fill[48];
    memset(fill, 0xAA, sizeof(fill));

    rb_put(&rb, fill, 48);   /* write 48 bytes */
    rb_get(&rb, fill, 48);   /* read all: tail = 48 */

    /* now write beyond buffer end to trigger wrap */
    uint8_t data[] = "Wrap test: 1234567890";  /* 23 bytes */
    uint32_t len = (uint32_t)strlen((char *)data);

    rb_put(&rb, data, len);  /* head wraps: 48 + 23 > 64, wraps to 7 */
    TEST_ASSERT(rb_available(&rb) == len, "available should match after wrap write");

    uint8_t readback[64];
    rb_get(&rb, readback, len);
    TEST_ASSERT(memcmp(data, readback, len) == 0, "data should survive wrap-around");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_full_buffer(void)
{
    printf("Test: full buffer ... ");
    int local_fail = failures;

    rb_flush(&rb);

    /* fill exactly */
    uint8_t fill[TEST_BUF_SIZE];
    memset(fill, 0xCC, sizeof(fill));
    uint32_t written = rb_put(&rb, fill, TEST_BUF_SIZE);
    TEST_ASSERT(written == TEST_BUF_SIZE, "should write full capacity");
    TEST_ASSERT(rb_is_full(&rb), "should be full");
    TEST_ASSERT(rb_free_space(&rb) == 0, "free space should be 0");

    /* try to write more */
    uint8_t extra[] = "extra";
    written = rb_put(&rb, extra, (uint32_t)strlen((char *)extra));
    TEST_ASSERT(written == 0, "should reject write when full");

    /* drain */
    rb_get(&rb, fill, TEST_BUF_SIZE);
    TEST_ASSERT(rb_is_empty(&rb), "should be empty after drain");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_peek(void)
{
    printf("Test: rb_peek ... ");
    int local_fail = failures;

    rb_flush(&rb);

    uint8_t data[] = "Peek test data";
    uint32_t len = (uint32_t)strlen((char *)data);

    rb_put(&rb, data, len);

    /* peek should return data without consuming */
    uint8_t peek_buf[64];
    uint32_t peeked = rb_peek(&rb, peek_buf, len);
    TEST_ASSERT(peeked == len, "peek should return all bytes");
    TEST_ASSERT(memcmp(data, peek_buf, len) == 0, "peek data should match");
    TEST_ASSERT(rb_available(&rb) == len, "data should still be available after peek");

    /* consume after peek */
    uint8_t read_buf[64];
    rb_get(&rb, read_buf, len);
    TEST_ASSERT(rb_is_empty(&rb), "should be empty after get following peek");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_discard(void)
{
    printf("Test: rb_discard ... ");
    int local_fail = failures;

    rb_flush(&rb);

    uint8_t data[32];
    memset(data, 0xDD, sizeof(data));
    rb_put(&rb, data, 32);

    /* discard half */
    rb_discard(&rb, 16);
    TEST_ASSERT(rb_available(&rb) == 16, "16 bytes should remain after discarding 16");

    /* discard more than available */
    rb_discard(&rb, 100);
    TEST_ASSERT(rb_is_empty(&rb), "should be empty after over-discard");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_advance(void)
{
    printf("Test: rb_advance_head / rb_advance_tail ... ");
    int local_fail = failures;

    rb_flush(&rb);

    /* simulate DMA RX: advance head */
    rb_advance_head(&rb, 20);
    TEST_ASSERT(rb_available(&rb) == 20, "20 bytes should be available");
    TEST_ASSERT(rb_free_space(&rb) == TEST_BUF_SIZE - 20, "free space should decrease");

    /* simulate DMA TX: advance tail */
    rb_advance_tail(&rb, 10);
    TEST_ASSERT(rb_available(&rb) == 10, "10 bytes should remain after tail advance");

    if (failures == local_fail) printf("PASSED\n");
}

static void test_contig_len(void)
{
    printf("Test: contiguous length ... ");
    int local_fail = failures;

    rb_flush(&rb);

    /* fill near the end */
    uint8_t fill[60];
    memset(fill, 0xEE, sizeof(fill));
    rb_put(&rb, fill, 60);
    rb_get(&rb, fill, 60);  /* tail = 60 */

    /* write 10 bytes: head wraps from 60 to 6 (60+10-64=6) */
    uint8_t data[10];
    memset(data, 0xFF, sizeof(data));
    rb_put(&rb, data, 10);

    /* available: 10, but contig from tail=60 is only 4 */
    TEST_ASSERT(rb_available(&rb) == 10, "available should be 10");
    TEST_ASSERT(rb_contig_read_len(&rb) == 4, "contiguous read from tail should be 4");

    if (failures == local_fail) printf("PASSED\n");
}

int main(void)
{
    printf("=== RingBuffer Unit Tests ===\n\n");

    test_init();
    test_put_get();
    test_wrap_around();
    test_full_buffer();
    test_peek();
    test_discard();
    test_advance();
    test_contig_len();

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
