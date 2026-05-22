# DMA + RingBuffer + FreeRTOS UART Communication Framework

[![Platform](https://img.shields.io/badge/platform-STM32F103ZET6-blue)]()

A high-performance, non-blocking UART communication framework for STM32 microcontrollers using DMA, lock-free ring buffers, and FreeRTOS task synchronization.

## Features

- **Zero CPU-wait TX**: CPU writes data to ring buffer and returns immediately; DMA handles the rest
- **Lossless RX**: DMA circular mode continuously receives; IDLE interrupt notifies tasks
- **Thread-safe**: FreeRTOS mutex per port allows multiple tasks to share one UART without data interleaving
- **Lock-free SPSC ring buffer**: No mutex in ISR path — deterministic interrupt latency
- **Configurable**: Buffer sizes, DMA modes, notification methods, all in one header
- **Portable**: Port abstraction layer isolates HAL specifics; easy to add STM32F4/H7 support

## Architecture

```
Application → uart_api.h ─→ driver/uart_dma.h ─→ port/stm32f1xx/uart_port.c ─→ STM32 HAL
                              │
                              └── ringbuffer/ringbuffer.h (lock-free SPSC)
```

## Directory Structure

```
├── config/uart_config.h         # All configuration macros
├── ringbuffer/ringbuffer.{h,c}  # SPSC lock-free circular buffer
├── driver/uart_dma.{h,c}        # DMA + ring buffer bridge layer
├── api/uart_api.{h,c}           # High-level UART API (blocking/async/printf)
├── port/
│   ├── uart_port.h              # Unified port interface
│   └── stm32f1xx/uart_port.c    # STM32F103ZET6 HAL implementation
├── example/
│   ├── echo_demo.c              # Simple echo demo
│   └── protocol_demo.c          # Protocol frame parser demo
├── test/
│   ├── test_ringbuffer.c        # Ring buffer unit tests (host-compilable)
│   └── test_uart_loopback.c     # Hardware loopback integration test
└── doc/                         # Detailed documentation (Chinese)
```

## Quick Start

```c
#include "uart_api.h"

void my_task(void *param)
{
    uint8_t buf[128];

    // 1. Init UART1 @ 115200 baud
    uart_init(0, 115200, 512, 1024);

    uart_printf(0, "Hello from DMA UART!\r\n");

    while (1) {
        // 2. Block until data arrives (1s timeout)
        int32_t n = uart_recv(0, buf, sizeof(buf), pdMS_TO_TICKS(1000));
        if (n > 0) {
            uart_send(0, buf, n, pdMS_TO_TICKS(100));
        }
    }
}
```

## Requirements

- STM32F103ZET6 (or compatible)
- STM32 HAL Library
- FreeRTOS v10.x or later
- GCC ARM Embedded toolchain

## Documentation

- [Framework Principles & Architecture (Chinese)](doc/框架原理.md)
- [Development Errors & Solutions (Chinese)](doc/开发错误与解决方案.md)
- [Code Implementation Details (Chinese)](doc/代码实现详解.md)
- [Original Project Spec (Chinese)](doc/项目说明.md)

## License

MIT License
