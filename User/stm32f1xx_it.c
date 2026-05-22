/**
 * stm32f1xx_it.c — ISR routing to DMA-UART Framework
 */

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_port.h"

void SysTick_Handler(void)
{
    HAL_IncTick();
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        xPortSysTickHandler();
    }
}

/* USART1 */
void USART1_IRQHandler(void)          { uart_port_irq_handler(UART_PORT1); }
void DMA1_Channel4_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT1); }
void DMA1_Channel5_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT1); }

/* USART2 */
void USART2_IRQHandler(void)          { uart_port_irq_handler(UART_PORT2); }
void DMA1_Channel7_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT2); }
void DMA1_Channel6_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT2); }

/* USART3 */
void USART3_IRQHandler(void)          { uart_port_irq_handler(UART_PORT3); }
void DMA1_Channel2_IRQHandler(void)   { uart_port_dma_tx_irq_handler(UART_PORT3); }
void DMA1_Channel3_IRQHandler(void)   { uart_port_dma_rx_irq_handler(UART_PORT3); }

void HardFault_Handler(void) { while (1); }
