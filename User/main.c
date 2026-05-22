/**
 * main.c — DMA + RingBuffer + FreeRTOS UART Demo
 *
 * UART1 (PA9/PA10) → Console 115200 8N1
 * UART2 (PA2/PA3)   → Loopback (需短接TX-RX)
 * PC13 → Heartbeat LED
 */
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_api.h"
#include <string.h>

/* ── System Clock: HSE 8MHz → PLL ×9 → 72MHz ── */
static void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0;

    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    FLASH->ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    RCC->CFGR |= RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PPRE2_DIV1;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    SystemCoreClock = 72000000;
    SysTick_Config(SystemCoreClock / 1000);
}

/* ── Echo Task (UART1 Console) ── */
static void echo_task(void *pv)
{
    uint8_t buf[128];
    uart_printf(0, "\r\n========================================\r\n");
    uart_printf(0, " DMA + RingBuffer + FreeRTOS UART Demo\r\n");
    uart_printf(0, " STM32F103ZET6  @  72MHz\r\n");
    uart_printf(0, " UART1=Console  115200\r\n");
    uart_printf(0, " UART2=Loopback 115200\r\n");
    uart_printf(0, " Cmds: info / test / help\r\n");
    uart_printf(0, "========================================\r\n> ");

    while (1) {
        int32_t n = uart_recv(0, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
        if (n > 0) {
            buf[n] = 0;
            if      (strncmp((char*)buf, "info", 4) == 0) {
                uart_printf(0, "\r\nSYSCLK=%luMHz  Heap=%lu  TXfree=%lu  RXavail=%lu\r\n",
                    SystemCoreClock/1000000, xPortGetFreeHeapSize(),
                    uart_tx_free(0), uart_rx_available(0));
            }
            else if (strncmp((char*)buf, "test", 4) == 0) {
                char *s = "Loopback OK!\r\n";
                uart_send(1, (uint8_t*)s, (uint32_t)strlen(s), pdMS_TO_TICKS(1000));
                vTaskDelay(pdMS_TO_TICKS(100));
                uint8_t rx[64]; int32_t m = uart_recv(1, rx, sizeof(rx), pdMS_TO_TICKS(200));
                uart_printf(0, "[Test] %s (%ld bytes)\r\n", (m > 0) ? "PASS" : "FAIL (check PA2-PA3 jumper)", m);
            }
            else { uart_printf(0, "echo: "); uart_send(0, buf, (uint32_t)n, pdMS_TO_TICKS(100)); }
            uart_printf(0, "\r\n> ");
        } else vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── Heartbeat LED (PC13) ── */
static void heartbeat_task(void *pv)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH = (GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13)) | GPIO_CRH_MODE13_1;
    TickType_t t = xTaskGetTickCount();
    while (1) { GPIOC->ODR ^= GPIO_ODR_ODR13; vTaskDelayUntil(&t, pdMS_TO_TICKS(500)); }
}

/* ── main ── */
int main(void)
{
    SystemClock_Config();
    uart_init(0, 115200, 512, 1024);  /* UART1: console */
    uart_init(1, 115200, 256, 512);   /* UART2: loopback */

    xTaskCreate(echo_task,      "echo",      256, NULL, 2, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", 128, NULL, 1, NULL);
    vTaskStartScheduler();
    while (1);
}

void vApplicationMallocFailedHook(void)     { __disable_irq(); while (1); }
void vApplicationStackOverflowHook(TaskHandle_t x, char *n) { (void)x; (void)n; __disable_irq(); while (1); }
