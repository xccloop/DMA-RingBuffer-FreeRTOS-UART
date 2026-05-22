# DMA + RingBuffer + FreeRTOS UART 通信框架

[![MCU](https://img.shields.io/badge/MCU-STM32F103ZET6-blue)](https://www.st.com)
[![RTOS](https://img.shields.io/badge/RTOS-FreeRTOS%20V10-green)](https://www.freertos.org)
[![Compiler](https://img.shields.io/badge/Compiler-Keil%20MDK--ARM%20V5-orange)](https://www.keil.com)
[![License](https://img.shields.io/badge/License-MIT-yellow)](LICENSE)

基于 STM32F103ZET6 的高性能 DMA 驱动多路 UART 通信框架，支持并发收发，CPU 零干预数据传输。

> **Author**: 向治昌

## 为什么需要这个框架？

传统 UART 通信方式有三个痛点：

| 方式 | 问题 |
|------|------|
| 轮询 | CPU 100% 占用等待每个字节 |
| 中断 | 每字节一次 ISR，高波特率下中断风暴（1Mbps ≈ 每秒 100,000 次中断） |
| 阻塞发送 | 发送 1KB @ 115200 需阻塞 ~89ms，RTOS 下浪费整个时间片 |

本框架通过 **DMA + 环形缓冲区 + FreeRTOS** 三层组合，使 CPU 只在开始和结束时参与，中间所有数据搬运由硬件自动完成。

## 特性

- **零 CPU 占用传输** — DMA 自动搬运数据，TX/RX 均无需 CPU 逐字节参与
- **多路并发** — 统一 API 管理 USART1/2/3，互斥锁保护多任务访问
- **变长帧支持** — USART IDLE 硬件中断检测帧边界，无需软件定时器
- **SPSC 无锁环形缓冲区** — ISR 与 Task 之间零临界区开销，head/tail 单调递增无回绕
- **阻塞 + 非阻塞 API** — 同时支持同步等待和异步轮询两种模式
- **线程安全** — 每个 UART 端口独立 mutex，多任务并发写入不交错
- **中断安全** — DMA/USART IRQ 优先级高于 FreeRTOS BASEPRI，不被临界区屏蔽

## 技术栈

| 层级 | 技术 |
|------|------|
| MCU | STM32F103ZET6 (Cortex-M3, 72MHz, 512KB Flash, 64KB SRAM) |
| 开发板 | 正点原子 战舰 V4 |
| RTOS | FreeRTOS V10 (heap_4) |
| HAL | STM32F1xx HAL (精简，仅 DMA/GPIO/UART/RCC/Cortex) |
| 编译器 | Keil MDK-ARM V5 (ARMCC V5.06) |

## 架构

```
User (main.c, stm32f1xx_it.c)
  └─ uart_api       (mutex-protected, blocking / non-blocking / printf)
       └─ uart_dma   (DMA TX chained + DMA RX circular + ringbuffer + ISR)
            └─ uart_port  (STM32F1 register-level HAL port)
                 └─ ringbuffer (SPSC lock-free)
```

### 数据流

```
TX: uart_printf → uart_send → ringbuffer → tx_kick → DMA1_CHx → USART_DR
     ↑ ISR on_tx_tc: advance ringbuffer tail, kick next segment if pending

RX: USART_DR → DMA1_CHx (circular) → ISR on_idle / on_rx_ht / on_rx_tc
     → rx_update (delta algorithm) → ringbuffer head advance
     → vTaskNotifyGiveFromISR → echo_task → uart_recv → ringbuffer get
```

### DMA 通道映射

| UART | TX DMA | RX DMA | 备注 |
|------|--------|--------|------|
| USART1 | DMA1_CH4 | DMA1_CH5 | Console (PA9/PA10 → CH340C USB-UART) |
| USART2 | DMA1_CH7 | DMA1_CH6 | Loopback test (PA2/PA3 jumpered) |
| USART3 | DMA1_CH2 | DMA1_CH3 | 预留 |

## API 速览

### 初始化

```c
#include "uart_api.h"

// 初始化 UART1: 115200bps, TX 512B buffer, RX 1024B buffer
uart_init(0, 115200, 512, 1024);

// 初始化 UART2（用于回环测试）
uart_init(1, 115200, 256, 512);
```

### 发送

```c
// 格式化输出（最常用，内部走 uart_send）
uart_printf(0, "Hello World!\r\n");
uart_printf(0, "System Clock: %lu MHz\r\n", SystemCoreClock / 1000000);

// 阻塞发送（带超时，tick 单位）
const char *data = "AT+PING\r\n";
uart_send(0, (uint8_t*)data, strlen(data), pdMS_TO_TICKS(1000));

// 非阻塞发送（RingBuffer 满则立即返回 -1）
uart_send_async(0, (uint8_t*)data, strlen(data));

// 等待所有数据发送完毕
uart_tx_flush(0, pdMS_TO_TICKS(5000));
```

### 接收

```c
uint8_t buf[128];

// 非阻塞读取（有多少读多少）
int32_t n = uart_recv_async(0, buf, sizeof(buf));
if (n > 0) {
    // 处理 buf[0..n-1]
}

// 阻塞读取（等待至少 1 字节，超时 1 秒）
n = uart_recv(0, buf, sizeof(buf), pdMS_TO_TICKS(1000));

// 查询可读字节数
uint32_t pending = uart_rx_available(0);
```

### 完整示例：命令行 Echo

```c
void echo_task(void *pv) {
    uint8_t buf[128];
    uart_printf(0, "> ");

    while (1) {
        uint32_t avail = uart_rx_available(0);
        if (avail > 0) {
            int32_t n = uart_recv_async(0, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = 0;
                uart_printf(0, "echo: %s\r\n> ", buf);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

## 功能完成情况

| 功能 | 状态 | 说明 |
|------|------|------|
| UART1 DMA 发送 | ✅ | uart_printf / uart_send 可用 |
| UART1 DMA 接收 | ✅ | IDLE 中断 + 环形缓冲区，支持变长帧 |
| UART2 回环测试 | ✅ | `UART2_loop` 命令，PA2-PA3 短接验证全链路 |
| 多路 UART 并发 | ✅ | USART1/2/3 统一 API，互斥锁保护 TX |
| FreeRTOS 集成 | ✅ | 心跳 LED (PB5/PE5 500ms) + echo 任务 |
| 中断优先级 | ✅ | DMA/USART IRQ 优先级 4，不受 FreeRTOS BASEPRI 屏蔽 |

## 性能

| 指标 | 数值 |
|------|------|
| uart_send 入队延迟 | ~200ns（仅 memcpy + 指针更新） |
| DMA 启动延迟 | ~100ns（写 CCR 寄存器） |
| IDLE 帧检测延迟 | 1 字节时间（硬件自动，零 CPU） |
| 发送完成通知延迟 | ~2μs（ISR → TaskNotify） |
| 框架 RAM 开销（3 端口） | ~3KB ~ 5KB（默认配置） |

## 快速开始

### 硬件连接

```
USB 线 → 开发板 USB UART 口 (CH340C, PA9/PA10)
ST-Link → 开发板 SWD 口 (PA13/PA14)
PA2 ←→ PA3 杜邦线短接 (可选，用于 UART2 回环测试)
```

### 编译与烧录

1. Keil MDK 打开 `Project.uvprojx`
2. Rebuild all target files
3. ST-Link 烧录 `Output/DMAFreeRTOS.hex`

### 串口连接

1. 打开 SSCOM（推荐）或任意串口工具
2. 端口号: 开发板对应的 COM 口
3. 波特率: **115200** / 数据位: 8 / 停止位: 1 / 校验: 无
4. **取消勾选 RTS 和 DTR**（否则板子复位后可能进入 bootloader）
5. 点"打开串口"

### 启动信息

上电或按 RESET 后，终端显示：

```
========================================
 DMA + RingBuffer + FreeRTOS UART
 STM32F103ZET6 @ 72 MHz
 UART1=Console  115200 8N1
 UART2=Loopback 115200 8N1
 Cmds: UART1 / UART2_loop (PA2-PA3 jumpered)
========================================
>
```

### 命令

| 命令 | 功能 |
|------|------|
| `UART1` | 查看 UART1 状态 (SYSCLK / Heap / TXfree / RX) |
| `UART2_loop` | UART2 回环测试 (需 PA2-PA3 短接) |
| 任意其他文本 | 回显 `echo: <输入内容>` |

### 示例

```
> UART1
  UART1 SYSCLK=72MHz Heap=9024 TXfree=512 RX=0
> hello
echo: hello
> UART2_loop
[UART2_loop] PASS: 'Loopback OK!'
>
```

## 目录结构

```
├── User/              main.c, stm32f1xx_it.c, stm32f1xx_hal_conf.h, stubs.c
├── Framework/         UART 框架层
│   ├── uart_api.c/h   API 层 (互斥锁、阻塞/非阻塞/printf)
│   ├── uart_dma.c/h   DMA 驱动层 (ringbuffer + ISR 回调)
│   ├── uart_port.c/h  寄存器端口层 (GPIO/USART/DMA/NVIC 初始化)
│   └── ringbuffer.c/h SPSC lock-free 环形缓冲区
├── FreeRTOS/          FreeRTOS V10 内核 + heap_4
├── HAL/               STM32F1xx HAL (精简)
├── System/            CMSIS 系统文件
├── Core/              CMSIS Core + 启动文件
└── doc/
    ├── 框架原理.md     完整技术原理 (DMA/RingBuffer/FreeRTOS 深度分析)
    ├── 代码实现详解.md  逐文件代码讲解
    ├── debug-journal.md 7 个 Bug 完整排查记录
    ├── 开发错误与解决方案.md 错误对照表
    └── 项目说明.md     项目背景与设计目标
```

## 已知限制

- `uart_recv` 的任务通知机制在 `tx_task` 为 NULL 时不可靠，当前使用轮询方式 (`uart_rx_available` + `uart_recv_async`) 替代
- `uart_tx_flush` 依赖 `tx_task` 通知，需在任务上下文中调用前先调用 `uart_dma_set_tx_task`
- 调度器启动前的 `uart_printf` 与任务内轮询 TX 存在竞态，需加延时或 DMA 完成等待
- 暂无硬件流控 (RTS/CTS) 支持
- 暂无运行时波特率切换 API

## 调试经验

完整的调试过程记录在 [doc/debug-journal.md](doc/debug-journal.md)，包含 7 个 Bug 的排查：

1. MobaXterm RTS/DTR → BOOT0 被拉高
2. DMA1 时钟未开启
3. NVIC 优先级被 FreeRTOS BASEPRI 屏蔽
4. GPIO 时钟在任务内延迟使能导致崩溃
5. `rx_update` 算法在 CNDTR 未变化时误推进 ringbuffer
6. `memset` 清零导致回调指针丢失
7. `uart_recv` 任务通知机制失效

关键方法论：**分层隔离**（轮询 TX → 裸 DMA → 框架 API）、**暴力诊断**（ISR 中直接发字符到 USART）。

## 许可证

MIT License — 详见 [LICENSE](LICENSE)

## 更多文档

- [框架原理](doc/框架原理.md) — DMA、RingBuffer、FreeRTOS 同步深度分析
- [代码实现详解](doc/代码实现详解.md) — 逐文件代码讲解
- [调试全记录](doc/debug-journal.md) — 7 个 Bug 的完整排查过程
