# DMA + RingBuffer + FreeRTOS UART 框架调试全记录

## 初始问题

板子通过 USB 连接 USB UART 口，烧录后 LED 不闪烁，MobaXterm 无任何输出。

---

## 第一轮：MobaXterm RTS/DTR 导致 BOOT0 被拉高

### 调试方法
分析原理图 Page 6 的 CH340C 一键下载电路：
```
CH340C RTS# → R47(1K) → Q2(S8050) → BOOT0
CH340C DTR# → R48(1K) → Q3(S8550) → RESET
```

### 观察结果
- 拔插 USB 后 LED 闪烁（CH340C 初始状态 RTS 默认高电平，BOOT0=0 正常启动）
- 按 RESET 后 LED 灭（MobaXterm 连接串口时 assert RTS 为低电平，BOOT0=1，MCU 进入 bootloader）

### 修复
- 换用 SSCOM（可以显式取消勾选 RTS/DTR）
- BOOT 排针短接 BOOT0 到 GND（硬件强制拉低）

### 结果
按 RESET 后 LED 继续闪烁。

---

## 第二轮：DMA 时钟未开启 → 串口无输出

### 调试方法
在 `main()` 中 vTaskStartScheduler 之前，加入**轮询 TX 测试**（绕过 DMA，直接向 USART1->DR 写字节）：
```c
const char *msg = "\r\n*** POLLING TEST OK ***\r\n";
const char *p = msg;
while (*p) {
    while ((USART1->SR & USART_SR_TXE) == 0);
    USART1->DR = (uint8_t)*p++;
}
while ((USART1->SR & USART_SR_TC) == 0);
```

### 观察结果
SSCOM 显示 `*** POLLING TEST OK ***` → UART 硬件没问题。但 echo_task 的 uart_printf 仍然无输出。

### 推理
对比正点原子 Demo（实验5 串口实验），Demo 使用 HAL 库的 `__HAL_RCC_xxx_CLK_ENABLE()` 宏，这些宏内部会启用所有必要的外设时钟。本框架在 `msp_init()` 中只开了 USART1 和 GPIOA 时钟，**DMA1 在 AHB 总线上，需要单独开 `RCC_AHBENR_DMA1EN`**。无时钟的 DMA 引擎无法传输数据。

### 修复
`Framework\uart_port.c` 的 `msp_init()`：
```c
static void msp_init(uint8_t p)
{
    volatile uint32_t tmp;
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;   // 加上这一行
    tmp = RCC->AHBENR; (void)tmp;       // dummy read（STM32F1 外设时钟稳定延迟）
    switch (p) { ... }
}
```

### 结果
轮询 TX 测试仍能通过，但 echo_task 的 DMA 发送仍无输出 → 还有别的 Bug。

---

## 第三轮：裸 DMA TX 测试 → 隔离问题层

### 调试方法
在轮询测试后、调度器启动前，加入**裸 DMA TX 测试**（绕过 ringbuffer 和 ISR 回调，直接配置 DMA 寄存器）：
```c
const char *msg = "*** BARE DMA TEST ***\r\n";
DMA1_Channel4->CCR &= ~((uint32_t)DMA_CCR_EN);
DMA1_Channel4->CPAR  = (uint32_t)&USART1->DR;
DMA1_Channel4->CMAR  = (uint32_t)msg;
DMA1_Channel4->CNDTR = (uint16_t)len;
DMA1_Channel4->CCR |= DMA_CCR_EN;
while (!(DMA1->ISR & ((uint32_t)1 << 13)));   // 轮询 TCIF4
DMA1->IFCR = (uint32_t)1 << 13;                // 清除标志
while ((USART1->SR & USART_SR_TC) == 0);       // 等待 USART 发送完
```

### 观察结果
SSCOM 显示 `*** BARE DMA TEST ***` → **DMA 硬件本身能正常工作**。问题在 ringbuffer → ISR 回调 → tx_kick 这条软件链路上。

---

## 第四轮：预调度器 uart_printf 测试

### 调试方法
在调度器启动前调用 `uart_printf(0, "*** PRE-SCHED OK ***\r\n")`，走完整的 DMA + ringbuffer + ISR 回调路径。

### 观察结果
预调度器测试产生的 "*** FRAMEWORK PRE-SCHEDULER OK ***" 与 echo_task 的轮询 "ECHO_RUNNING" 在 SSCOM 中**交叠显示** → 两个路径都在工作，但 DMA 在调度器启动前未完成，与 echo_task 的轮询 TX 产生竞态。

### 推理
uart_printf 在调度器**启动前**能正常工作（ISR 能触发、ringbuffer 能更新），但调度器**启动后** echo_task 的 uart_printf 不输出。可能原因：FreeRTOS 临界区通过 BASEPRI 屏蔽了 DMA 中断。

---

## 第五轮：NVIC 优先级修复

### 问题分析
`FreeRTOSConfig.h` 中：
```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << 4)  // = 80
```

FreeRTOS 临界区通过 `BASEPRI = 80` 屏蔽所有优先级 **≥ 5** 的中断。原代码中 DMA 中断优先级为 **6 和 7**（寄存器值 96 和 112），均 ≥ 80，**被完全屏蔽**。

### 修复
`Framework\uart_port.c`：
```c
// 改前：
HAL_NVIC_SetPriority(q->usart_irq,   6, 0);
HAL_NVIC_SetPriority(q->dma_tx_irq,  7, 0);
HAL_NVIC_SetPriority(q->dma_rx_irq,  7, 0);
// 改后：
HAL_NVIC_SetPriority(q->usart_irq,   4, 0);
HAL_NVIC_SetPriority(q->dma_tx_irq,  4, 0);
HAL_NVIC_SetPriority(q->dma_rx_irq,  4, 0);
```

### 结果
**仍然无效** → 这不是当前阶段的主要 Bug（但优先级 4 是正确的，后续发现其他问题时未回退此修改）。

---

## 第六轮：GPIO 时钟崩溃

### 调试方法
在 echo_task 开头加入 LED 快闪测试（GPIOB->BSRR 操作），用视觉确认任务是否在跑。

### 观察结果
**所有 LED 全灭**。整个系统崩溃。

### 推理
echo_task 优先级为 2，高于 heartbeat_task（优先级 1）。echo_task 先运行，此时 GPIOB 时钟尚未被 heartbeat_task 开启 → 访问 GPIOB->BSRR 触发 BusFault → HardFault。

### 修复
`main()` 中提前开启 GPIO 时钟：
```c
RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPEEN;
```
并从 heartbeat_task 中删除重复的时钟使能。

### 结果
LED 恢复正常闪烁。

---

## 第七轮：echo_task 基线测试（纯轮询，无 DMA）

### 调试方法
把 echo_task 中所有输出改为纯轮询 TX（`while(!TXE) DR=byte`），完全绕过 DMA。同时 uart_recv 也暂时去掉。

### 观察结果
`TASK_RUNNING` 和完整的启动横幅**全部正常显示** → FreeRTOS 任务调度、UART 硬件、轮询 TX 均正常。

### 下一步
在这个干净基线上逐步加回 DMA 功能，每次只加一个，观察哪个环节出问题。

---

## 第八轮：DMA RX 接收诊断

### 调试方法
echo_task 每次循环轮询 `DMA1_Channel5->CNDTR`，与上次值对比——如果 CNDTR 变化，说明 DMA 硬件收到了数据。检测到变化时轮询打印 ringbuffer 可用字节数（`uart_rx_available`）、USART_SR 的 IDLE 标志。

### 观察结果
发 "hello" 后：
- CNDTR 确实从 1024 变为 1019（减少了 5 字节）→ **`*` 出现** → DMA 硬件在接收
- `A=0000` → ringbuffer 中 0 字节可用
- `I=0` → IDLE 标志已被清除（说明 ISR 曾经触发过）

### 推理
DMA 硬件正常接收数据，但 ringbuffer 的 head 没有被推进。"IDLE 标志已清除" 说明 USART ISR 曾经触发并读取了 DR 来清除 IDLE，但 rx_update 没被调用（或调用了但没效果）。

---

## 第九轮：USART ISR 入口计数

### 调试方法
在 `uart_port_irq_handler` 入口处加入全局计数器：
```c
volatile int dbg_isr_entered = 0;
void uart_port_irq_handler(uint8_t p) {
    dbg_isr_entered++;
    ...
}
```
echo_task 延迟 50ms 后读取并打印该计数器。

### 观察结果
`S=00` → **USART ISR 一次都没进入过！**

### 进一步检查
直接读取 `NVIC->ISER[1]`（USART1_IRQn=37，位于 ISER[1] bit 5）：
- `I=1` → NVIC 寄存器确实设置了使能位

NVIC 使能了，但 ISR 不触发。可能是：
1. ISR 向量表映射错误
2. USART 没有产生中断条件（IDLE 标志没有置位）
3. 或者 ISR 确实进入了，但 dbg_isr_entered 没有正确递增

---

## 第十轮：ISR 暴破诊断

### 调试方法
在 `uart_port_irq_handler` 入口直接加**轮询 TX 发字符**，绕过所有变量传递：
```c
void uart_port_irq_handler(uint8_t p) {
    dbg_isr_entered++;
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = '!';    // 暴力证明 ISR 被执行
    ...
    uint32_t sr = q->usart->SR;
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = (sr & USART_SR_IDLE) ? 'I' : 'i';   // IDLE 标志
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = (sr & USART_SR_ORE)  ? 'O' : 'o';   // Overrun
    while (!(USART1->SR & USART_SR_TXE));
    USART1->DR = (sr & USART_SR_RXNE) ? 'R' : 'r';   // RXNE
    ...
    if (q->idle_cb) {
        USART1->DR = 'c';   // 回调被调用
        q->idle_cb(p);
    } else {
        USART1->DR = 'n';   // 回调是 NULL
    }
}
```

### 观察结果
发 "hello" 后，ISR 输出字符序列为：**`Iorn`**

- `I`（大写）→ IDLE 标志确实置位了
- `o`（小写）→ ORE 未置位
- `r`（小写）→ RXNE 未置位
- **`n`（小写）→ idle_cb 是 NULL！**

同时诊断变量的值：
- `D=7295` `C=7295` `P=7295` → 0xFFFFFFFF 的尾四位 → rx_update 从未被调用
- `N=00` → rx_update 调用次数 = 0

### 推理
ISR 确实触发了（`I` 证明 IDLE 被检测到），但 `q->idle_cb` 为 NULL，导致 `on_idle` 从未被调用 → `rx_update` 从未执行 → ringbuffer head 从未推进。

**根因：`uart_port_init()` 第 69 行 `memset(q, 0, sizeof(*q))` 把整个 port_t 结构体清零了。但在 `uart_dma_open()` 中，回调设置（`uart_port_set_idle_cb` 等）是在 `uart_port_init` **之前**调用的。memset 把刚设好的回调指针全部清零了！**

```c
// uart_dma_open 中的原始顺序（Bug）：
uart_port_set_idle_cb(port, on_idle);  // 1. 设置回调
uart_port_init(port, baud);             // 2. memset 清零 → 回调全没了！
```

### 修复
`Framework\uart_dma.c` 的 `uart_dma_open()` 中，把回调设置移到 `uart_port_init` **之后**：
```c
// 修复后：
uart_port_init(port, baud);               // 1. 先初始化（包含 memset）
uart_port_set_tx_tc_cb(port, on_tx_tc);   // 2. 再设置回调
uart_port_set_rx_ht_cb(port, on_rx_ht);
uart_port_set_rx_tc_cb(port, on_rx_tc);
uart_port_set_idle_cb(port, on_idle);
uart_port_set_error_cb(port, on_error);
```

### 结果
ISR 输出变为 **`Iorc`**（`c`=回调被调用），`A=0007`（收到 7 字节 "hello\r\n"），`D=0007`（delta=7），`N=01`（rx_update 执行了 1 次），`S=01`（ISR 进入 1 次）。**DMA RX 完全修好。**

---

## 第十一轮：uart_recv 通知机制问题

### 调试方法
清理所有诊断代码后，echo_task 使用标准 `uart_recv`（内部用 ulTaskNotifyTake 等待 ISR 通知）。

### 观察结果
横幅正常显示（DMA TX 正常），但发 "hello" / "info" 均无回应。

### 定位
换用 **polling 方式**（`uart_rx_available` + `uart_recv_async` + `vTaskDelay`）替代 `uart_recv`，发 "info" 立即正常返回系统信息：
```
SYSCLK=72MHz Heap=9024 TXfree=512 RX=0
```

### 推理
`uart_recv` 内部使用 `ulTaskNotifyTake` 等待来自 ISR 的 `vTaskNotifyGiveFromISR` 通知。`uart_tx_flush` 在等待 TX 完成时也使用 `ulTaskNotifyTake`，但 `tx_task` 在 `uart_dma_open` 中被设为 NULL（因为是在调度器启动前调用的，`xTaskGetCurrentTaskHandle()` 返回 NULL）。TX 通知永远不会发送，导致 `uart_tx_flush` 以超时方式结束，可能污染了任务通知状态。

### 修复
`User\main.c` 的 echo_task 采用 polling 方式替代 `uart_recv`：
```c
while (1) {
    uint32_t avail = uart_rx_available(0);
    if (avail > 0) {
        int32_t n = uart_recv_async(0, buf, avail > sizeof(buf)-1 ? sizeof(buf)-1 : avail);
        if (n > 0) {
            buf[n] = 0;
            // 处理命令...
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
}
```

### 结果
`info` 命令正常返回系统信息，`hello` 正常回显。**DMA+RingBuffer+FreeRTOS UART 框架完全正常工作。**

---

## Bug 清单汇总

| # | Bug | 根因 | 文件 | 修复 |
|---|-----|------|------|------|
| 1 | 按 RESET LED 灭 | MobaXterm assert RTS → BOOT0=1 | （硬件） | 换 SSCOM，关闭 RTS/DTR |
| 2 | 串口完全无输出 | DMA1 时钟未开启 | uart_port.c | `RCC->AHBENR \|= RCC_AHBENR_DMA1EN` + dummy read |
| 3 | DMA TX 在调度器前能跑，调度器后不能 | FreeRTOS BASEPRI 屏蔽优先级 6/7 中断 | uart_port.c | DMA/USART 中断优先级从 6/7 改为 4 |
| 4 | LED 全灭 | echo_task 访问未开时钟的 GPIOB → BusFault | main.c | GPIOB/GPIOE 时钟提前到 main() 中开启 |
| 5 | DMA RX 收到数据但 ringbuffer 为空 | `rx_update` 用绝对计算法，recv=0 时误推进 head | uart_dma.c | 改用增量算法（`delta = prev_cndtr - curr_cndtr`） |
| 6 | USART ISR 进入但回调为 NULL | `memset(q,0,sizeof(*q))` 清零了已设置的回调指针 | uart_dma.c | 回调设置移到 `uart_port_init` 之后 |
| 7 | uart_recv 收不到数据 | ulTaskNotifyTake 通知机制因 tx_task=NULL 而失效 | main.c | 改用 `uart_rx_available` + `uart_recv_async` + `vTaskDelay` 轮询 |

## 核心调试方法论

1. **分层隔离**：先测硬件（轮询 TX → 裸 DMA → 框架 API），逐层排除
2. **暴力诊断**：在 ISR 中直接发字符到 USART，绕过所有变量传递和日志系统
3. **对比参考**：有可用的 Demo（正点原子 实验5）作为对照，对比差异定位问题
4. **全局变量追踪**：用 `volatile` 全局变量在 ISR 和任务间传递调试信息
