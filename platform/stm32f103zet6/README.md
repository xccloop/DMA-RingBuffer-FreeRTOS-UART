# STM32F103ZET6 集成指南

**本工程已自包含（Self-Contained）。所有第三方源码（FreeRTOS、HAL、CMSIS）均已打包在 `ThirdParty/` 目录中。无需下载任何外部依赖。**

---

## 目录说明

```
platform/stm32f103zet6/
├── main.c                       # 演示程序 (双串口回显 + 状态监控)
├── stm32f1xx_it.c               # 中断向量挂接 (USART + DMA ISR → 框架)
├── startup_stm32f103xe.s        # CMSIS 启动文件 (向量表 + 复位)
├── FreeRTOSConfig.h             # FreeRTOS 内核配置
├── stm32f1xx_hal_conf.h         # STM32 HAL 模块使能配置
├── STM32F103XE_FLASH.sct        # Keil MDK 散列文件 (linker)
├── STM32F103XE_FLASH.ld         # GCC 链接脚本
├── Makefile                     # GCC Makefile (自包含，所有路径本地)
├── DMAFreeRTOS.uvprojx          # Keil MDK 工程文件 (自包含)
├── DMAFreeRTOS.uvoptx           # Keil MDK 工程选项
├── configure_paths.ps1          # 验证脚本 (检查文件完整性)
├── ThirdParty/                  # ★ 所有第三方库已打包
│   ├── FreeRTOS/                # FreeRTOS Kernel v11.x
│   │   ├── src/                 # tasks.c, queue.c, list.c, timers.c
│   │   ├── include/             # FreeRTOS.h, task.h, queue.h, semphr.h...
│   │   └── portable/
│   │       ├── RVDS/ARM_CM3/    # port.c (Keil/ARMCC)
│   │       └── MemMang/         # heap_4.c
│   ├── HAL/                     # STM32F1 HAL Driver
│   │   ├── Src/                 # 8 个 .c 文件 (hal, gpio, dma, uart, rcc...)
│   │   └── Inc/                 # 对应 .h 文件
│   └── CMSIS/
│       ├── Device/              # stm32f1xx.h, stm32f103xe.h, system_stm32f1xx.c
│       └── Include/             # core_cm3.h, cmsis_gcc.h, cmsis_armcc.h...
└── README.md                    # 本文档
```

---

## 前提条件

编译工具（二选一）：

- **Keil MDK-ARM** v5.38+ (商业软件)
- **ARM GCC** (arm-none-eabi-gcc) — 免费

Keil 用户还需安装 STM32F1 设备包：
1. 打开 Keil → Pack Installer
2. 搜索 `STM32F1`
3. 安装 `Keil::STM32F1xx_DFP`（设备支持 + Flash 算法）

---

## 快速开始

### 检查文件完整性

```powershell
cd platform\stm32f103zet6
.\configure_paths.ps1
```

### 方式 A：Keil MDK（推荐）

```
1. 双击 DMAFreeRTOS.uvprojx
2. F7 = Build
3. F8 = Download (ST-Link)
4. Ctrl+F5 = Start Debug
```

就这么简单。所有源文件路径均已配好，开箱即用。

### 方式 B：GCC Makefile

```bash
cd platform/stm32f103zet6
make clean && make          # 编译
make flash                  # 烧录 (需 st-flash)
```

### 方式 C：CubeMX 集成

1. CubeMX 选 STM32F103ZE，配时钟 + USART + FreeRTOS
2. 把 `ringbuffer/`, `driver/`, `api/`, `port/`, `config/` 复制到工程
3. 用 `platform/stm32f103zet6/main.c` 和 `stm32f1xx_it.c` 替换 CubeMX 生成的
4. 添加 include paths → 编译

---

## 硬件连接

### UART1（Console）

| STM32F103ZET6 | USB-to-TTL 模块 |
|---------------|-----------------|
| PA9 (TX)      | RX              |
| PA10 (RX)     | TX              |
| GND           | GND             |

终端设置：115200 baud, 8 data bits, 1 stop bit, no parity

### UART2（Loopback Test）

用杜邦线短接：**PA2 ↔ PA3**

### 状态 LED

PC13（常见开发板上已连接 LED）

---

## 验证项目工作

烧录后，打开串口终端（115200 8N1），应该看到：

```
╔══════════════════════════════════════╗
║  DMA + RingBuffer + FreeRTOS UART   ║
║  STM32F103ZET6 @ 72MHz             ║
╠══════════════════════════════════════╣
║  UART1: Console (115200 8N1)        ║
║  UART2: Loopback (9600 8N1)         ║
╠══════════════════════════════════════╣
║  Commands:                          ║
║   'info' → System status            ║
║   'test' → Run loopback test        ║
║   'help' → This menu                ║
╚══════════════════════════════════════╝

>
```

- 输入 `info` + Enter → 显示系统状态（SYSCLK、堆剩余、缓冲区使用率）
- 输入 `test` + Enter → 通过 UART2 回环测试（需短接 PA2-PA3）
- 输入任何其他字符 → 回显（echo）

---

## 常见问题

| 问题 | 解决 |
|------|------|
| Keil 找不到 stm32f1xx_hal.h | include path 中缺少 HAL Inc 路径 |
| Keil 找不到 FreeRTOS.h | include path 中缺少 FreeRTOS include 路径 |
| 编译有 undefined reference to `uart_port_*` | 未将 port/stm32f1xx/uart_port.c 加入工程 |
| 编译有 implicit declaration of `xSemaphore*` | 未将 FreeRTOS/Source/include 加入 include path |
| 运行时在 prvTaskExitError 卡住 | FreeRTOSConfig.h 缺少或不正确 |
| 运行时收不到 UART1 数据 | 检查 PA9/PA10 接线；确认波特率匹配 |
| UART2 loopback 测试失败 | 检查 PA2-PA3 短接；确认 UART2 初始化成功 |
| HardFault | 检查 NVIC 优先级是否 < configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY |
| PC13 LED 不闪烁 | 某些开发板 PC13 需要低电平点亮 |
