#******************************************************************************
# @file     Makefile
# @brief    GCC ARM Embedded build for STM32F103ZET6 DMA-UART Framework
#
# Usage:
#   1. Install ARM GCC: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain
#   2. Set paths below to your STM32CubeF1 and FreeRTOS directories
#   3. make clean && make
#   4. Flash with: make flash (uses ST-Link via openocd or st-flash)
#******************************************************************************

PROJECT = DMAFreeRTOS

# ---- Toolchain ----
CROSS   = arm-none-eabi-
CC      = $(CROSS)gcc
AS      = $(CROSS)gcc -x assembler-with-cpp
OBJCOPY = $(CROSS)objcopy
OBJDUMP = $(CROSS)objdump
SIZE    = $(CROSS)size

# ---- STM32CubeF1 path (ADJUST THIS) ----
STM32CUBE = C:/Users/$(USERNAME)/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5
# If you don't have Cube, download from:
# https://github.com/STMicroelectronics/STM32CubeF1

# ---- FreeRTOS path (ADJUST THIS) ----
FREERTOS = C:/FreeRTOS/FreeRTOSv202212.01/FreeRTOS
# Download from: https://github.com/FreeRTOS/FreeRTOS

# ---- Project root ----
PROJROOT = ../..

# ---- MCU flags ----
CPU       = -mcpu=cortex-m3
FPU       =
FLOAT_ABI =
MCU       = $(CPU) -mthumb $(FPU) $(FLOAT_ABI)

# ---- Defines ----
DEFS  = -DSTM32F103xE
DEFS += -DUSE_HAL_DRIVER

# ---- Includes ----
INC   = -I$(PROJROOT)
INC  += -I$(PROJROOT)/config
INC  += -I$(PROJROOT)/ringbuffer
INC  += -I$(PROJROOT)/driver
INC  += -I$(PROJROOT)/api
INC  += -I$(PROJROOT)/port
INC  += -I.

# STM32CubeF1 HAL/CMSIS includes
INC  += -I$(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Inc
INC  += -I$(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy
INC  += -I$(STM32CUBE)/Drivers/CMSIS/Device/ST/STM32F1xx/Include
INC  += -I$(STM32CUBE)/Drivers/CMSIS/Include

# FreeRTOS includes
INC  += -I$(FREERTOS)/Source/include
INC  += -I$(FREERTOS)/Source/portable/GCC/ARM_CM3

# ---- Flags ----
CFLAGS  = $(MCU) $(DEFS) $(INC)
CFLAGS += -Wall -Wextra -O2 -g
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-common -fmessage-length=0
CFLAGS += -std=c11

ASFLAGS = $(MCU) $(DEFS) $(INC) -Wall -g

LDFLAGS  = $(MCU) -T STM32F103XE_FLASH.ld
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -Wl,--gc-sections -Wl,-Map=$(PROJECT).map,--cref
LDFLAGS += -Wl,--print-memory-usage

# ---- Sources ----

# Framework
C_SRC  = $(PROJROOT)/ringbuffer/ringbuffer.c
C_SRC += $(PROJROOT)/port/stm32f1xx/uart_port.c
C_SRC += $(PROJROOT)/driver/uart_dma.c
C_SRC += $(PROJROOT)/api/uart_api.c

# Platform
C_SRC += main.c
C_SRC += stm32f1xx_it.c
ASM_SRC = startup_stm32f103xe.s

# HAL (minimal set)
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c
C_SRC += $(STM32CUBE)/Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_pwr.c

# CMSIS system
C_SRC += $(STM32CUBE)/Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c

# FreeRTOS
C_SRC += $(FREERTOS)/Source/tasks.c
C_SRC += $(FREERTOS)/Source/queue.c
C_SRC += $(FREERTOS)/Source/list.c
C_SRC += $(FREERTOS)/Source/timers.c
C_SRC += $(FREERTOS)/Source/portable/MemMang/heap_4.c
C_SRC += $(FREERTOS)/Source/portable/GCC/ARM_CM3/port.c

# ---- Objects ----
OBJ = $(C_SRC:.c=.o) $(ASM_SRC:.s=.o)

# ---- Targets ----
all: $(PROJECT).elf

$(PROJECT).elf: $(OBJ)
	@echo "  LD    $@"
	@$(CC) $(LDFLAGS) -o $@ $^
	@$(SIZE) $@
	@$(OBJCOPY) -O ihex   $@ $(PROJECT).hex
	@$(OBJCOPY) -O binary $@ $(PROJECT).bin

%.o: %.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(PROJECT).elf $(PROJECT).hex $(PROJECT).bin $(PROJECT).map

flash: $(PROJECT).bin
	st-flash write $(PROJECT).bin 0x08000000

.PHONY: all clean flash
