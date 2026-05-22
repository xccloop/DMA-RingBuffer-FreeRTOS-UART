/**
 * @file    stm32f1xx_it.c
 * @brief   STM32F1 interrupt handlers
 *
 * Route USART and DMA interrupts to the framework's port layer.
 * Include this file in your project instead of the auto-generated one.
 */

#include "uart_port.h"

/* ── USART1 (Console) ── */
void USART1_IRQHandler(void)          { uart_port_irq_handler(UART_PORT1); }
void DMA1_Channel4_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT1); }
void DMA1_Channel5_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT1); }

/* ── USART2 (Loopback / Protocol) ── */
void USART2_IRQHandler(void)          { uart_port_irq_handler(UART_PORT2); }
void DMA1_Channel7_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT2); }
void DMA1_Channel6_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT2); }

/* ── USART3 ── */
void USART3_IRQHandler(void)          { uart_port_irq_handler(UART_PORT3); }
void DMA1_Channel2_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT3); }
void DMA1_Channel3_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT3); }

/* ── SysTick (for FreeRTOS) ── */
void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* ── HardFault (debug aid) ── */
void HardFault_Handler(void)
{
    __disable_irq();
    while (1);
}
