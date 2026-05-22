# STM32F103ZET6 集成指南

本目录包含将 DMA-UART 框架部署到 STM32F103ZET6 所需的全部平台文件。

---

## 目录说明

```
platform/stm32f103zet6/
├── main.c                    # 演示程序 (双串口回显 + 状态监控)
├── stm32f1xx_it.c            # 中断向量挂接 (USART + DMA ISR → 框架)
├── startup_stm32f103xe.s     # CMSIS 启动文件 (向量表 + 复位)
├── FreeRTOSConfig.h          # FreeRTOS 内核配置
├── stm32f1xx_hal_conf.h      # STM32 HAL 模块使能配置
├── STM32F103XE_FLASH.sct     # Keil MDK 散列文件 (linker)
├── STM32F103XE_FLASH.ld      # GCC 链接脚本
├── Makefile                  # GCC Makefile (arm-none-eabi-gcc)
├── DMAFreeRTOS.uvprojx       # Keil MDK 工程文件
├── DMAFreeRTOS.uvoptx        # Keil MDK 工程选项
├── configure_paths.ps1       # 自动路径配置脚本 (PowerShell)
└── README.md                 # 本文档
```

---

## 前提条件

需要获取以下第三方库（不在本仓库中）：

### 1. STM32CubeF1（HAL 库 + CMSIS）

**方式 A** — 从 ST 官网下载：
1. 访问 https://www.st.com/stm32cubef1
2. 下载 STM32Cube_FW_F1 包
3. 解压到任意目录

**方式 B** — 从 GitHub 克隆：
```bash
git clone https://github.com/STMicroelectronics/STM32CubeF1.git
```

### 2. FreeRTOS

```bash
git clone https://github.com/FreeRTOS/FreeRTOS.git --depth 1
```

### 3. 编译器（二选一）

- **Keil MDK-ARM** v5.38+ (商业软件，需许可证)
- **ARM GCC** (GNU Arm Embedded Toolchain) — 免费
  - 下载: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain

---

## 快速开始

### 步骤 1：配置路径

运行自动配置脚本：

```powershell
cd platform\stm32f103zet6
.\configure_paths.ps1
```

如果未自动找到 STM32CubeF1 或 FreeRTOS，手动指定：

```powershell
.\configure_paths.ps1 -CubePath "D:\STM32Cube_FW_F1_V1.8.5" -FreeRtosPath "D:\FreeRTOSv202212.01\FreeRTOS"
```

脚本会自动更新 Makefile 和 Keil 工程文件中的路径。

---

### 方式 A：使用 Keil MDK（推荐新手）

#### A.1 打开工程

1. 双击 `DMAFreeRTOS.uvprojx` 在 Keil MDK 中打开
2. 如果弹出 "Missing path" 警告，说明 STM32CubeF1/FreeRTOS 路径不正确
3. 重新运行 `configure_paths.ps1` 修正路径

#### A.2 配置工程选项（如需要）

右键 Target → **Options for Target 'DMAFreeRTOS'**：

| 选项卡 | 设置 |
|--------|------|
| **Target** | XTAL = 8.0 MHz, ARM Compiler = Use default |
| **C/C++** | Optimize = -O2, C99 mode |
| **Linker** | Scatter File = `.\STM32F103XE_FLASH.sct` |
| **Debug** | Use: ST-Link Debugger |
| **Utilities** | Use Debug Driver for Flash Programming |

#### A.3 构建 & 烧录

- **Build**: F7
- **Download**: F8
- **Start Debug**: Ctrl+F5

#### A.4 导入 HAL 源文件（如果自动路径不生效）

Keil .uvprojx 使用 `__CUBE__` 和 `__FREERTOS__` 占位符。如果 configure_paths.ps1 未替换它们，你需要：

1. 在 Keil 中右键每个 Group → **Add Existing Files**
2. 手动从 STM32CubeF1 目录添加 HAL .c 文件
3. 手动从 FreeRTOS 目录添加 tasks.c, queue.c, list.c, timers.c, port.c, heap_4.c

或者，用 CubeMX 生成一个基础工程后，**只需添加框架文件**：
- `ringbuffer/ringbuffer.c`
- `port/stm32f1xx/uart_port.c`
- `driver/uart_dma.c`
- `api/uart_api.c`
- `platform/stm32f103zet6/main.c`（替换 CubeMX 生成的 main.c）
- `platform/stm32f103zet6/stm32f1xx_it.c`（替换 CubeMX 生成的 it.c）

#### A.5 包含路径配置

在 Options for Target → C/C++ → Include Paths 中添加：

```
..\..                       ; 项目根目录
..\..\config                ; uart_config.h
..\..\ringbuffer            ; ringbuffer.h
..\..\driver                ; uart_dma.h
..\..\api                   ; uart_api.h
..\..\port                  ; uart_port.h
.                           ; FreeRTOSConfig.h, stm32f1xx_hal_conf.h
<STM32CubeF1>\Drivers\STM32F1xx_HAL_Driver\Inc
<STM32CubeF1>\Drivers\CMSIS\Device\ST\STM32F1xx\Include
<STM32CubeF1>\Drivers\CMSIS\Include
<FreeRTOS>\Source\include
<FreeRTOS>\Source\portable\RVDS\ARM_CM3
```

---

### 方式 B：使用 GCC Makefile（免费，适合 CI/CD）

#### B.1 安装 ARM GCC

```bash
# 下载并解压到 C:\arm-gcc
# 添加到 PATH
set PATH=C:\arm-gcc\bin;%PATH%
```

#### B.2 编译

```bash
cd platform\stm32f103zet6
make clean
make
```

产物：
- `DMAFreeRTOS.elf` — ELF 调试文件
- `DMAFreeRTOS.hex` — Intel Hex 格式
- `DMAFreeRTOS.bin` — 原始二进制

#### B.3 烧录（ST-Link）

```bash
# 方式 1：用 st-flash (https://github.com/stlink-org/stlink)
make flash

# 方式 2：用 OpenOCD
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program DMAFreeRTOS.elf verify reset exit"
```

---

### 方式 C：用 STM32CubeMX 生成基础工程 + 添加框架

这是最可靠的方式，推荐所有用户：

1. **打开 STM32CubeMX**，选择芯片 STM32F103ZETx
2. **配置时钟**：HSE 8MHz → PLL ×9 → 72MHz SYSCLK
3. **配置外设**：
   - USART1: Mode = Asynchronous, 参数默认
   - USART2: Mode = Asynchronous, 参数默认
   - 两个 USART 都 **不勾选** "Generate IRQ Handler"（用框架提供的 it.c）
4. **配置 FreeRTOS**：Middleware → FreeRTOS → Enabled
   - Interface: CMSIS_V2
   - Total heap size: 15360
5. **生成代码**：Project → Generate Code
6. **替换/合并文件**：
   - 复制 `ringbuffer/`, `driver/`, `api/`, `port/`, `config/` 到工程目录
   - 用 `platform/stm32f103zet6/stm32f1xx_it.c` 覆盖 CubeMX 生成的 it.c
   - 用 `platform/stm32f103zet6/main.c` 覆盖 CubeMX 生成的 main.c
   - 保留 CubeMX 生成的 `FreeRTOSConfig.h`（或与平台目录中的合并）
   - 将 `port/uart_port.h`, `config/uart_config.h` 路径加入 include path
7. **编译验证**

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
