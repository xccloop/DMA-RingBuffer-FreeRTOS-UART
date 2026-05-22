/**
 * main.c — DMA + RingBuffer + FreeRTOS UART Demo
 *
 * UART1 (PA9/PA10) → USB-TTL Console 115200 8N1
 * UART2 (PA2/PA3)   → Loopback test (optional)
 * PC13 → Heartbeat LED
 */

#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "uart_api.h"
#include <string.h>

/* ── Robust Clock Init: try HSE, fallback to HSI ── */
static void SystemClock_Config(void)
{
    uint32_t timeout;

    /* Enable HSI and wait */
    RCC->CR |= RCC_CR_HSION;
    for (timeout = 0; timeout < 100000; timeout++) {
        if (RCC->CR & RCC_CR_HSIRDY) break;
    }

    /* Try HSE */
    RCC->CR |= RCC_CR_HSEON;
    for (timeout = 0; timeout < 0x100000; timeout++) {
        if (RCC->CR & RCC_CR_HSERDY) break;
    }

    if (RCC->CR & RCC_CR_HSERDY) {
        /* HSE OK → PLL ×9 = 72MHz */
        FLASH->ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;
        RCC->CFGR = RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC   /* HSE as PLL src */
                  | RCC_CFGR_HPRE_DIV1                     /* AHB /1 */
                  | RCC_CFGR_PPRE1_DIV2                    /* APB1 /2 = 36MHz */
                  | RCC_CFGR_PPRE2_DIV1;                   /* APB2 /1 = 72MHz */

        RCC->CR |= RCC_CR_PLLON;
        for (timeout = 0; timeout < 100000; timeout++) {
            if (RCC->CR & RCC_CR_PLLRDY) break;
        }
        if (RCC->CR & RCC_CR_PLLRDY) {
            RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
            while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
            RCC->CR &= ~RCC_CR_HSION;  /* save power */
            SystemCoreClock = 72000000;
        }
    } else {
        /* No HSE → use HSI 8MHz */
        RCC->CR &= ~RCC_CR_HSEON;      /* disable HSE */
        FLASH->ACR = FLASH_ACR_LATENCY_0 | FLASH_ACR_PRFTBE;
        RCC->CFGR = RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV1 | RCC_CFGR_PPRE2_DIV1;
        SystemCoreClock = 8000000;
    }

    SysTick_Config(SystemCoreClock / 1000);
}

/* ── Echo Task (UART1) ── */
static void echo_task(void *pv)
{
    uint8_t buf[128];
    (void)pv;

    uart_printf(0, "\r\n");
    uart_printf(0, "========================================\r\n");
    uart_printf(0, " DMA + RingBuffer + FreeRTOS UART Demo\r\n");
    uart_printf(0, " STM32F103ZET6 @ %lu MHz\r\n", SystemCoreClock / 1000000);
    uart_printf(0, " UART1=Console  115200 8N1\r\n");
    uart_printf(0, " UART2=Loopback 115200 8N1\r\n");
    uart_printf(0, " Cmds: info / test\r\n");
    uart_printf(0, "========================================\r\n> ");

    while (1) {
        int32_t n = uart_recv(0, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
        if (n > 0) {
            buf[n] = 0;
            if      (strncmp((char*)buf, "info", 4) == 0) {
                uart_printf(0, "\r\nSYSCLK=%luMHz Heap=%lu TXfree=%lu RX=%lu\r\n",
                    SystemCoreClock / 1000000,
                    (uint32_t)xPortGetFreeHeapSize(),
                    uart_tx_free(0), uart_rx_available(0));
            }
            else if (strncmp((char*)buf, "test", 4) == 0) {
                char *s = "Loopback OK!\r\n";
                uart_send(1, (uint8_t*)s, (uint32_t)strlen(s), pdMS_TO_TICKS(1000));
                vTaskDelay(pdMS_TO_TICKS(100));
                uint8_t rx[64];
                int32_t m = uart_recv(1, rx, sizeof(rx), pdMS_TO_TICKS(200));
                uart_printf(0, "[Test] %s (%ld bytes)\r\n",
                    (m > 0) ? "PASS" : "FAIL (check PA2-PA3 jumper)", m);
            }
            else {
                uart_printf(0, "echo: ");
                uart_send(0, buf, (uint32_t)n, pdMS_TO_TICKS(100));
            }
            uart_printf(0, "\r\n> ");
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* ── Heartbeat LED (PB5 + PE5) ── */
static void heartbeat_task(void *pv)
{
    (void)pv;
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPEEN;
    /* PB5: push-pull output, 2MHz */
    GPIOB->CRL = (GPIOB->CRL & ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5))
               | GPIO_CRL_MODE5_1;
    /* PE5: push-pull output, 2MHz */
    GPIOE->CRL = (GPIOE->CRL & ~(GPIO_CRL_CNF5 | GPIO_CRL_MODE5))
               | GPIO_CRL_MODE5_1;
    TickType_t t = xTaskGetTickCount();
    while (1) {
        GPIOB->ODR ^= GPIO_ODR_ODR5;
        GPIOE->ODR ^= GPIO_ODR_ODR5;
        vTaskDelayUntil(&t, pdMS_TO_TICKS(500));
    }
}

/* ── main ── */
int main(void)
{
    HAL_Init();                        /* NVIC priority group + SysTick */
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
