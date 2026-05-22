;******************************************************************************
; @file     startup_stm32f103xe.s
; @brief    CMSIS Cortex-M3 startup for STM32F103xE (Keil ARM Compiler)
;           Works with armcc (v5) and armclang (v6) integrated assembler
;******************************************************************************

                PRESERVE8
                THUMB

; Vector Table mapped to address 0 at reset
                AREA    RESET, DATA, READONLY
                EXPORT  __Vectors
                EXPORT  __Vectors_End
                EXPORT  __Vectors_Size

__Vectors       DCD     __initial_sp              ; Top of Stack
                DCD     Reset_Handler             ; Reset Handler
                DCD     NMI_Handler               ; NMI Handler
                DCD     HardFault_Handler         ; Hard Fault Handler
                DCD     MemManage_Handler         ; MPU Fault Handler
                DCD     BusFault_Handler          ; Bus Fault Handler
                DCD     UsageFault_Handler        ; Usage Fault Handler
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     0                         ; Reserved
                DCD     SVC_Handler               ; SVCall Handler
                DCD     DebugMon_Handler          ; Debug Monitor Handler
                DCD     0                         ; Reserved
                DCD     PendSV_Handler            ; PendSV Handler
                DCD     SysTick_Handler           ; SysTick Handler

                ; External Interrupts (STM32F103xE)
                DCD     WWDG_IRQHandler
                DCD     PVD_IRQHandler
                DCD     TAMPER_IRQHandler
                DCD     RTC_IRQHandler
                DCD     FLASH_IRQHandler
                DCD     RCC_IRQHandler
                DCD     EXTI0_IRQHandler
                DCD     EXTI1_IRQHandler
                DCD     EXTI2_IRQHandler
                DCD     EXTI3_IRQHandler
                DCD     EXTI4_IRQHandler
                DCD     DMA1_Channel1_IRQHandler
                DCD     DMA1_Channel2_IRQHandler
                DCD     DMA1_Channel3_IRQHandler
                DCD     DMA1_Channel4_IRQHandler
                DCD     DMA1_Channel5_IRQHandler
                DCD     DMA1_Channel6_IRQHandler
                DCD     DMA1_Channel7_IRQHandler
                DCD     ADC1_2_IRQHandler
                DCD     USB_HP_CAN1_TX_IRQHandler
                DCD     USB_LP_CAN1_RX0_IRQHandler
                DCD     CAN1_RX1_IRQHandler
                DCD     CAN1_SCE_IRQHandler
                DCD     EXTI9_5_IRQHandler
                DCD     TIM1_BRK_IRQHandler
                DCD     TIM1_UP_IRQHandler
                DCD     TIM1_TRG_COM_IRQHandler
                DCD     TIM1_CC_IRQHandler
                DCD     TIM2_IRQHandler
                DCD     TIM3_IRQHandler
                DCD     TIM4_IRQHandler
                DCD     I2C1_EV_IRQHandler
                DCD     I2C1_ER_IRQHandler
                DCD     I2C2_EV_IRQHandler
                DCD     I2C2_ER_IRQHandler
                DCD     SPI1_IRQHandler
                DCD     SPI2_IRQHandler
                DCD     USART1_IRQHandler
                DCD     USART2_IRQHandler
                DCD     USART3_IRQHandler
                DCD     EXTI15_10_IRQHandler
                DCD     RTCAlarm_IRQHandler
                DCD     USBWakeUp_IRQHandler
                DCD     TIM8_BRK_IRQHandler
                DCD     TIM8_UP_IRQHandler
                DCD     TIM8_TRG_COM_IRQHandler
                DCD     TIM8_CC_IRQHandler
                DCD     ADC3_IRQHandler
                DCD     FSMC_IRQHandler
                DCD     SDIO_IRQHandler
                DCD     TIM5_IRQHandler
                DCD     SPI3_IRQHandler
                DCD     UART4_IRQHandler
                DCD     UART5_IRQHandler
                DCD     TIM6_IRQHandler
                DCD     TIM7_IRQHandler
                DCD     DMA2_Channel1_IRQHandler
                DCD     DMA2_Channel2_IRQHandler
                DCD     DMA2_Channel3_IRQHandler
                DCD     DMA2_Channel4_5_IRQHandler
__Vectors_End

__Vectors_Size  EQU     __Vectors_End - __Vectors

;==============================================================================
; Reset Handler — called at power-on
;==============================================================================
                AREA    |.text|, CODE, READONLY

Reset_Handler   PROC
                EXPORT  Reset_Handler             [WEAK]
                IMPORT  SystemInit
                IMPORT  __main

                LDR     R0, =SystemInit
                BLX     R0
                LDR     R0, =__main
                BX      R0
                ENDP

;==============================================================================
; Default Exception Handlers (infinite loop — override as needed)
;==============================================================================
NMI_Handler                PROC
                EXPORT  NMI_Handler               [WEAK]
                B       .
                ENDP
HardFault_Handler          PROC
                EXPORT  HardFault_Handler         [WEAK]
                B       .
                ENDP
MemManage_Handler          PROC
                EXPORT  MemManage_Handler         [WEAK]
                B       .
                ENDP
BusFault_Handler           PROC
                EXPORT  BusFault_Handler          [WEAK]
                B       .
                ENDP
UsageFault_Handler         PROC
                EXPORT  UsageFault_Handler        [WEAK]
                B       .
                ENDP
SVC_Handler                PROC
                EXPORT  SVC_Handler               [WEAK]
                B       .
                ENDP
DebugMon_Handler           PROC
                EXPORT  DebugMon_Handler          [WEAK]
                B       .
                ENDP
PendSV_Handler             PROC
                EXPORT  PendSV_Handler            [WEAK]
                B       .
                ENDP
SysTick_Handler            PROC
                EXPORT  SysTick_Handler           [WEAK]
                B       .
                ENDP

;==============================================================================
; Default IRQ Handlers
;==============================================================================
Default_Handler PROC
                EXPORT  WWDG_IRQHandler                   [WEAK]
                EXPORT  PVD_IRQHandler                    [WEAK]
                EXPORT  TAMPER_IRQHandler                 [WEAK]
                EXPORT  RTC_IRQHandler                    [WEAK]
                EXPORT  FLASH_IRQHandler                  [WEAK]
                EXPORT  RCC_IRQHandler                    [WEAK]
                EXPORT  EXTI0_IRQHandler                  [WEAK]
                EXPORT  EXTI1_IRQHandler                  [WEAK]
                EXPORT  EXTI2_IRQHandler                  [WEAK]
                EXPORT  EXTI3_IRQHandler                  [WEAK]
                EXPORT  EXTI4_IRQHandler                  [WEAK]
                EXPORT  DMA1_Channel1_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel2_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel3_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel4_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel5_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel6_IRQHandler          [WEAK]
                EXPORT  DMA1_Channel7_IRQHandler          [WEAK]
                EXPORT  ADC1_2_IRQHandler                 [WEAK]
                EXPORT  USB_HP_CAN1_TX_IRQHandler         [WEAK]
                EXPORT  USB_LP_CAN1_RX0_IRQHandler        [WEAK]
                EXPORT  CAN1_RX1_IRQHandler               [WEAK]
                EXPORT  CAN1_SCE_IRQHandler               [WEAK]
                EXPORT  EXTI9_5_IRQHandler                [WEAK]
                EXPORT  TIM1_BRK_IRQHandler               [WEAK]
                EXPORT  TIM1_UP_IRQHandler                [WEAK]
                EXPORT  TIM1_TRG_COM_IRQHandler           [WEAK]
                EXPORT  TIM1_CC_IRQHandler                [WEAK]
                EXPORT  TIM2_IRQHandler                   [WEAK]
                EXPORT  TIM3_IRQHandler                   [WEAK]
                EXPORT  TIM4_IRQHandler                   [WEAK]
                EXPORT  I2C1_EV_IRQHandler                [WEAK]
                EXPORT  I2C1_ER_IRQHandler                [WEAK]
                EXPORT  I2C2_EV_IRQHandler                [WEAK]
                EXPORT  I2C2_ER_IRQHandler                [WEAK]
                EXPORT  SPI1_IRQHandler                   [WEAK]
                EXPORT  SPI2_IRQHandler                   [WEAK]
                EXPORT  USART1_IRQHandler                 [WEAK]
                EXPORT  USART2_IRQHandler                 [WEAK]
                EXPORT  USART3_IRQHandler                 [WEAK]
                EXPORT  EXTI15_10_IRQHandler              [WEAK]
                EXPORT  RTCAlarm_IRQHandler               [WEAK]
                EXPORT  USBWakeUp_IRQHandler              [WEAK]
                EXPORT  TIM8_BRK_IRQHandler               [WEAK]
                EXPORT  TIM8_UP_IRQHandler                [WEAK]
                EXPORT  TIM8_TRG_COM_IRQHandler           [WEAK]
                EXPORT  TIM8_CC_IRQHandler                [WEAK]
                EXPORT  ADC3_IRQHandler                   [WEAK]
                EXPORT  FSMC_IRQHandler                   [WEAK]
                EXPORT  SDIO_IRQHandler                   [WEAK]
                EXPORT  TIM5_IRQHandler                   [WEAK]
                EXPORT  SPI3_IRQHandler                   [WEAK]
                EXPORT  UART4_IRQHandler                  [WEAK]
                EXPORT  UART5_IRQHandler                  [WEAK]
                EXPORT  TIM6_IRQHandler                   [WEAK]
                EXPORT  TIM7_IRQHandler                   [WEAK]
                EXPORT  DMA2_Channel1_IRQHandler          [WEAK]
                EXPORT  DMA2_Channel2_IRQHandler          [WEAK]
                EXPORT  DMA2_Channel3_IRQHandler          [WEAK]
                EXPORT  DMA2_Channel4_5_IRQHandler        [WEAK]

WWDG_IRQHandler
PVD_IRQHandler
TAMPER_IRQHandler
RTC_IRQHandler
FLASH_IRQHandler
RCC_IRQHandler
EXTI0_IRQHandler
EXTI1_IRQHandler
EXTI2_IRQHandler
EXTI3_IRQHandler
EXTI4_IRQHandler
DMA1_Channel1_IRQHandler
DMA1_Channel2_IRQHandler
DMA1_Channel3_IRQHandler
DMA1_Channel4_IRQHandler
DMA1_Channel5_IRQHandler
DMA1_Channel6_IRQHandler
DMA1_Channel7_IRQHandler
ADC1_2_IRQHandler
USB_HP_CAN1_TX_IRQHandler
USB_LP_CAN1_RX0_IRQHandler
CAN1_RX1_IRQHandler
CAN1_SCE_IRQHandler
EXTI9_5_IRQHandler
TIM1_BRK_IRQHandler
TIM1_UP_IRQHandler
TIM1_TRG_COM_IRQHandler
TIM1_CC_IRQHandler
TIM2_IRQHandler
TIM3_IRQHandler
TIM4_IRQHandler
I2C1_EV_IRQHandler
I2C1_ER_IRQHandler
I2C2_EV_IRQHandler
I2C2_ER_IRQHandler
SPI1_IRQHandler
SPI2_IRQHandler
USART1_IRQHandler
USART2_IRQHandler
USART3_IRQHandler
EXTI15_10_IRQHandler
RTCAlarm_IRQHandler
USBWakeUp_IRQHandler
TIM8_BRK_IRQHandler
TIM8_UP_IRQHandler
TIM8_TRG_COM_IRQHandler
TIM8_CC_IRQHandler
ADC3_IRQHandler
FSMC_IRQHandler
SDIO_IRQHandler
TIM5_IRQHandler
SPI3_IRQHandler
UART4_IRQHandler
UART5_IRQHandler
TIM6_IRQHandler
TIM7_IRQHandler
DMA2_Channel1_IRQHandler
DMA2_Channel2_IRQHandler
DMA2_Channel3_IRQHandler
DMA2_Channel4_5_IRQHandler
                B       .
                ENDP

;==============================================================================
; Stack and Heap Configuration
;==============================================================================
Stack_Size      EQU     0x00001000    ; 4 KB stack
Heap_Size       EQU     0x00000800    ; 2 KB heap (for libc; FreeRTOS uses heap_4)

                AREA    STACK, NOINIT, READWRITE, ALIGN=3
Stack_Mem       SPACE   Stack_Size
__initial_sp

                AREA    HEAP, NOINIT, READWRITE, ALIGN=3
Heap_Mem        SPACE   Heap_Size
__heap_limit

                ALIGN

;==============================================================================
; Stack / Heap initialization (when NOT using microlib)
;==============================================================================
                IF      :DEF:__MICROLIB

                EXPORT  __initial_sp
                EXPORT  __heap_base
                EXPORT  __heap_limit

                ELSE

                IMPORT  __use_two_region_memory
                EXPORT  __user_initial_stackheap

__user_initial_stackheap
                LDR     R0, =  Heap_Mem
                LDR     R1, =(Stack_Mem + Stack_Size)
                LDR     R2, = (Heap_Mem +  Heap_Size)
                LDR     R3, = Stack_Mem
                BX      LR

                ALIGN

                ENDIF

                END
