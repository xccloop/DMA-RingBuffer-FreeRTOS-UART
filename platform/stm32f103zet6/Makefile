#******************************************************************************
# @file     Makefile (self-contained)
# @brief    GCC ARM Embedded build — NO external paths needed
#
# All third-party sources are bundled in ./ThirdParty/
#******************************************************************************

PROJECT = DMAFreeRTOS

# ---- Toolchain ----
CROSS   = arm-none-eabi-
CC      = $(CROSS)gcc
AS      = $(CROSS)gcc -x assembler-with-cpp
OBJCOPY = $(CROSS)objcopy
SIZE    = $(CROSS)size

# ---- Project root ----
ROOT = ../..

# ---- MCU flags ----
CPU       = -mcpu=cortex-m3 -mthumb
DEFS      = -DSTM32F103xE -DUSE_HAL_DRIVER

# ---- All include paths (all local!) ----
INC  = -I$(ROOT)
INC += -I$(ROOT)/config
INC += -I$(ROOT)/ringbuffer
INC += -I$(ROOT)/driver
INC += -I$(ROOT)/api
INC += -I$(ROOT)/port
INC += -I.                           # FreeRTOSConfig.h, stm32f1xx_hal_conf.h
INC += -IThirdParty/FreeRTOS/include
INC += -IThirdParty/FreeRTOS/portable/RVDS/ARM_CM3
INC += -IThirdParty/HAL/Inc
INC += -IThirdParty/HAL/Inc/Legacy
INC += -IThirdParty/CMSIS/Device
INC += -IThirdParty/CMSIS/Include

# ---- Flags ----
CFLAGS  = $(CPU) $(DEFS) $(INC)
CFLAGS += -Wall -Wextra -O2 -g
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-common -std=c11
CFLAGS += -Wno-unused-parameter

ASFLAGS = $(CPU) $(DEFS) $(INC) -Wall -g

LDFLAGS  = $(CPU) -T STM32F103XE_FLASH.ld
LDFLAGS += -specs=nosys.specs -specs=nano.specs
LDFLAGS += -Wl,--gc-sections -Wl,-Map=$(PROJECT).map,--cref

# ---- Framework sources ----
C_SRC  = $(ROOT)/ringbuffer/ringbuffer.c
C_SRC += $(ROOT)/port/stm32f1xx/uart_port.c
C_SRC += $(ROOT)/driver/uart_dma.c
C_SRC += $(ROOT)/api/uart_api.c

# ---- Platform sources ----
C_SRC += main.c
C_SRC += stm32f1xx_it.c

# ---- HAL sources (bundled) ----
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_gpio.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_dma.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_uart.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_rcc.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_cortex.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_flash.c
C_SRC += ThirdParty/HAL/Src/stm32f1xx_hal_pwr.c

# ---- CMSIS sources (bundled) ----
C_SRC += ThirdParty/CMSIS/Device/system_stm32f1xx.c

# ---- FreeRTOS sources (bundled) ----
C_SRC += ThirdParty/FreeRTOS/src/tasks.c
C_SRC += ThirdParty/FreeRTOS/src/queue.c
C_SRC += ThirdParty/FreeRTOS/src/list.c
C_SRC += ThirdParty/FreeRTOS/src/timers.c
C_SRC += ThirdParty/FreeRTOS/portable/RVDS/ARM_CM3/port.c
C_SRC += ThirdParty/FreeRTOS/portable/MemMang/heap_4.c

# ---- Assembly sources ----
ASM_SRC = startup_stm32f103xe.s

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
	@echo "  Done: $(PROJECT).elf $(PROJECT).hex $(PROJECT).bin"

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
