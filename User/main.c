/**
 * main.c — DMA + RingBuffer + FreeRTOS UART Demo
 *
 * UART1 (PA9/PA10) → USB-TTL Console 115200 8N1
 * UART2 (PA2/PA3)   → Loopback test (optional)
 */

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_api.h"
#include <string.h>

static void SystemClock_Config(void)
{
    uint32_t timeout;
    RCC->CR |= RCC_CR_HSION;
    for (timeout = 0; timeout < 100000; timeout++) {
        if (RCC->CR & RCC_CR_HSIRDY) break;
    }
    RCC->CR |= RCC_CR_HSEON;
    for (timeout = 0; timeout < 0x100000; timeout++) {
        if (RCC->CR & RCC_CR_HSERDY) break;
    }
    if (RCC->CR & RCC_CR_HSERDY) {
        FLASH->ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;
        RCC->CFGR = RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC
                  | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;
        RCC->CR |= RCC_CR_PLLON;
        for (timeout = 0; timeout < 100000; timeout++) {
            if (RCC->CR & RCC_CR_PLLRDY) break;
        }
        if (RCC->CR & RCC_CR_PLLRDY) {
            RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
            while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
            RCC->CR &= ~RCC_CR_HSION;
            SystemCoreClock = 72000000;
        }
    } else {
        RCC->CR &= ~RCC_CR_HSEON;
        FLASH->ACR = FLASH_ACR_LATENCY_0 | FLASH_ACR_PRFTBE;
        RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;
        SystemCoreClock = 8000000;
    }
    SysTick_Config(SystemCoreClock / 1000);
}

static void echo_task(void *pv)
{
    uint8_t buf[128];
    (void)pv;

    uart_printf(0, "\r\n");
    uart_printf(0, "========================================\r\n");
    uart_printf(0, " DMA + RingBuffer + FreeRTOS UART\r\n");
    uart_printf(0, " STM32F103ZET6 @ %lu MHz\r\n", SystemCoreClock / 1000000);
    uart_printf(0, " UART1=Console  115200 8N1\r\n");
    uart_printf(0, " UART2=Loopback 115200 8N1\r\n");
    uart_printf(0, " Cmds: UART1 / UART2_loop (PA2-PA3 jumpered)\r\n");
    uart_printf(0, "========================================\r\n> ");

    /* wait for banner DMA to finish (simple delay, avoids uart_tx_flush notification issue) */
    vTaskDelay(pdMS_TO_TICKS(50));

    while (1) {
        /* poll rx_available first — working codepath */
        uint32_t avail = uart_rx_available(0);
        if (avail > 0) {
            int32_t n = uart_recv_async(0, buf, avail > sizeof(buf)-1 ? sizeof(buf)-1 : avail);
            if (n > 0) {
                buf[n] = 0;
                if (!strncmp((char*)buf, "UART1", 5))
                    uart_printf(0, "\r\n  UART1 SYSCLK=%luMHz Heap=%lu TXfree=%lu RX=%lu\r\n",
                        SystemCoreClock / 1000000, (uint32_t)xPortGetFreeHeapSize(),
                        uart_tx_free(0), uart_rx_available(0));
                else if (!strncmp((char*)buf, "UART2_loop", 10)) {
                    char *s = "Loopback OK!\r\n";
                    uart_send(1, (uint8_t*)s, (uint32_t)strlen(s), pdMS_TO_TICKS(1000));
                    vTaskDelay(pdMS_TO_TICKS(200));
                    uint8_t rx[64];
                    int32_t m = uart_recv_async(1, rx, sizeof(rx) - 1);
                    if (m > 0) {
                        rx[m] = 0;
                        uart_printf(0, "[UART2_loop] PASS: '%s'", rx);
                    } else {
                        uart_printf(0, "[UART2_loop] FAIL -- is PA2-PA3 jumper connected?");
                    }
                }
                else
                    uart_printf(0, "echo: %s", buf);
                uart_printf(0, "\r\n> ");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void heartbeat_task(void *pv)
{
    (void)pv;
    GPIOB->CRL = (GPIOB->CRL & ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5)) | GPIO_CRL_MODE5_1;
    GPIOE->CRL = (GPIOE->CRL & ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5)) | GPIO_CRL_MODE5_1;
    TickType_t t = xTaskGetTickCount();
    while (1) {
        GPIOB->ODR ^= GPIO_ODR_ODR5;
        GPIOE->ODR ^= GPIO_ODR_ODR5;
        vTaskDelayUntil(&t, pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPEEN;

    uart_init(0, 115200, 512, 1024);
    uart_init(1, 115200, 256, 512);

    xTaskCreate(echo_task,      "echo",      256, NULL, 2, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 128, NULL, 1, NULL);

    vTaskStartScheduler();
    while (1);
}

void vApplicationMallocFailedHook(void)     { __disable_irq(); while (1); }
void vApplicationStackOverflowHook(TaskHandle_t x, char *n) { (void)x; (void)n; __disable_irq(); while (1); }
