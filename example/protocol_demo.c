/**
 * @file    protocol_demo.c
 * @brief   Protocol frame parsing demo using DMA + RingBuffer + FreeRTOS
 *
 * Demonstrates:
 * - Frame-based reception using UART IDLE + callback
 * - Non-blocking send with uart_send_async
 * - Multi-port usage (UART1 for console, UART2 for protocol)
 *
 * Protocol: simple <STX><LEN><PAYLOAD><ETX><CRC> framing
 * - STX: 0x02 (1 byte)
 * - LEN: payload length (1 byte, max 250)
 * - PAYLOAD: variable length
 * - ETX: 0x03 (1 byte)
 * - CRC: XOR of all bytes except CRC (1 byte)
 */

#include "uart_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* ---- protocol constants ---- */
#define PROTOCOL_PORT        1       /* UART2 */
#define PROTOCOL_BAUDRATE    115200
#define FRAME_BUF_SIZE       256

#define STX                  0x02
#define ETX                  0x03

/* frame parser state machine */
typedef enum {
    STATE_IDLE,
    STATE_STX,
    STATE_LEN,
    STATE_PAYLOAD,
    STATE_ETX,
    STATE_CRC
} frame_state_t;

typedef struct {
    frame_state_t state;
    uint8_t       buf[FRAME_BUF_SIZE];
    uint8_t       payload_len;
    uint8_t       idx;
    uint8_t       crc;
} frame_parser_t;

static frame_parser_t g_parser;

/* ---- helpers ---- */

static uint8_t calc_crc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
    }
    return crc;
}

static void send_response(uint8_t port, const uint8_t *payload, uint8_t len)
{
    uint8_t frame[FRAME_BUF_SIZE];
    frame[0] = STX;
    frame[1] = len;
    memcpy(&frame[2], payload, len);
    frame[2 + len] = ETX;
    frame[3 + len] = calc_crc(frame, 3 + len);

    uart_send_async(port, frame, 4 + len);
}

/* ---- character-by-character frame parser ---- */

static void parse_frame(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uint8_t ch = data[i];

        switch (g_parser.state) {

        case STATE_IDLE:
            if (ch == STX) {
                g_parser.state = STATE_LEN;
                g_parser.crc   = ch;  /* STX included in CRC */
                g_parser.idx   = 0;
            }
            break;

        case STATE_LEN:
            g_parser.payload_len = ch;
            g_parser.crc        ^= ch;
            if (g_parser.payload_len == 0) {
                g_parser.state = STATE_ETX;
            } else if (g_parser.payload_len >= FRAME_BUF_SIZE) {
                /* invalid length */
                g_parser.state = STATE_IDLE;
            } else {
                g_parser.state = STATE_PAYLOAD;
                g_parser.idx   = 0;
            }
            break;

        case STATE_PAYLOAD:
            g_parser.buf[g_parser.idx++] = ch;
            g_parser.crc                ^= ch;
            if (g_parser.idx >= g_parser.payload_len) {
                g_parser.state = STATE_ETX;
            }
            break;

        case STATE_ETX:
            if (ch == ETX) {
                g_parser.crc   ^= ch;
                g_parser.state  = STATE_CRC;
            } else {
                /* invalid: reset */
                g_parser.state = STATE_IDLE;
            }
            break;

        case STATE_CRC:
            if (ch == g_parser.crc) {
                /* valid frame received */
                send_response(PROTOCOL_PORT, g_parser.buf, g_parser.payload_len);
            }
            g_parser.state = STATE_IDLE;
            break;
        }
    }
}

/* ---- IDLE callback: invoked when UART IDLE interrupt fires ---- */

static void on_frame_received(uint8_t port, const uint8_t *data, uint32_t len, void *arg)
{
    (void)port;
    (void)arg;

    /* parse received raw bytes through protocol state machine */
    parse_frame(data, len);
}

/* ---- protocol task ---- */

#define PROTOCOL_TASK_STACK_SIZE   512

void protocol_task(void *param)
{
    (void)param;

    /* init protocol parser */
    memset(&g_parser, 0, sizeof(g_parser));
    g_parser.state = STATE_IDLE;

    /* init UART2 for protocol communication */
    uart_init(PROTOCOL_PORT, PROTOCOL_BAUDRATE, 256, 512);

    /* set IDLE callback: entire frame delivered at once */
    uart_set_rx_callback(PROTOCOL_PORT, on_frame_received, NULL);

    /* console log */
    uart_printf(0, "[Protocol] UART2 initialized @ %lu bps\r\n",
        (uint32_t)PROTOCOL_BAUDRATE);

    while (1) {
        /* block until IDLE event, then callback is invoked */
        uart_yield(PROTOCOL_PORT, pdMS_TO_TICKS(500));
        /* yield returned: either data arrived or timeout */
    }
}

void protocol_demo_init(void)
{
    TaskHandle_t task_handle;
    xTaskCreate(
        protocol_task,
        "protocol",
        PROTOCOL_TASK_STACK_SIZE,
        NULL,
        tskIDLE_PRIORITY + 3,
        &task_handle
    );
}
