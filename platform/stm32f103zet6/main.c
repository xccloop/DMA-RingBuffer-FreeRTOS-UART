/**
 * @file    main.c
 * @brief   Main application: DMA + RingBuffer + FreeRTOS UART demo
 *
 * Demonstrates:
 * - Dual UART (UART1=console 115200, UART2=loopback 9600)
 * - Console echo task via uart_recv/uart_send
 * - Periodic status reporting via uart_printf
 * - Async protocol response via uart_send_async
 *
 * Hardware: STM32F103ZET6
 *   UART1: PA9(TX) / PA10(RX)  →  USB-to-TTL adapter
 *   UART2: PA2(TX) / PA3(RX)   →  jumper for loopback test
 *   HSE: 8MHz crystal → PLL → 72MHz SYSCLK
 */

#include "uart_api.h"
#include "FreeRTOS.h"
#include "task.h"

/* ================================================================
 * System Clock Configuration (HSE 8MHz → PLL → 72MHz)
 * ================================================================ */
static void SystemClock_Config(void)
{
    /* Reset RCC */
    RCC->CR |= RCC_CR_HSION;
    RCC->CFGR = 0;

    /* Enable HSE (8MHz external crystal) */
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY));

    /* Configure flash latency: 2 wait states for 72MHz (max 48MHz=0WS, 72MHz=2WS) */
    FLASH->ACR = FLASH_ACR_LATENCY_2 | FLASH_ACR_PRFTBE;

    /*
     * PLL config: HSE × 9 = 72MHz
     * PLLMUL = 0111 (×9), PLLSRC = 1 (HSE), PLLXTPRE = 0 (HSE not divided)
     */
    RCC->CFGR |= RCC_CFGR_PLLMULL9 | RCC_CFGR_PLLSRC;
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;    /* AHB = SYSCLK / 1 */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;   /* APB1 = HCLK / 2 = 36MHz */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;   /* APB2 = HCLK / 1 = 72MHz */

    /* Enable PLL and wait for lock */
    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY));

    /* Switch system clock to PLL */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    /* Disable HSI to save power */
    RCC->CR &= ~RCC_CR_HSION;

    SystemCoreClock = 72000000;
    SysTick_Config(SystemCoreClock / 1000);  /* 1ms tick for FreeRTOS */
}

/* ================================================================
 * Task: Console Echo (UART1)
 * ================================================================ */
#define ECHO_STACK_SIZE     256

static void echo_task(void *param)
{
    uint8_t buf[128];

    uart_printf(UART_PORT1, "\r\n");
    uart_printf(UART_PORT1, "╔══════════════════════════════════════╗\r\n");
    uart_printf(UART_PORT1, "║  DMA + RingBuffer + FreeRTOS UART   ║\r\n");
    uart_printf(UART_PORT1, "║  STM32F103ZET6 @ 72MHz             ║\r\n");
    uart_printf(UART_PORT1, "╠══════════════════════════════════════╣\r\n");
    uart_printf(UART_PORT1, "║  UART1: Console (115200 8N1)        ║\r\n");
    uart_printf(UART_PORT1, "║  UART2: Loopback (9600 8N1)         ║\r\n");
    uart_printf(UART_PORT1, "╠══════════════════════════════════════╣\r\n");
    uart_printf(UART_PORT1, "║  Commands:                          ║\r\n");
    uart_printf(UART_PORT1, "║   'info' → System status            ║\r\n");
    uart_printf(UART_PORT1, "║   'test' → Run loopback test        ║\r\n");
    uart_printf(UART_PORT1, "║   'help' → This menu                ║\r\n");
    uart_printf(UART_PORT1, "╚══════════════════════════════════════╝\r\n");
    uart_printf(UART_PORT1, "\r\n> ");

    while (1) {
        int32_t n = uart_recv(UART_PORT1, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));

        if (n > 0) {
            buf[n] = '\0';

            /* simple command parser */
            if (n >= 4 && buf[0] == 'i' && buf[1] == 'n' && buf[2] == 'f' && buf[3] == 'o') {
                uart_printf(UART_PORT1, "\r\n--- System Info ---\r\n");
                uart_printf(UART_PORT1, "  SYSCLK:    %lu MHz\r\n", SystemCoreClock / 1000000);
                uart_printf(UART_PORT1, "  FreeRTOS:  tick=%lu Hz\r\n", (uint32_t)configTICK_RATE_HZ);
                uart_printf(UART_PORT1, "  Heap free: %lu bytes\r\n", (uint32_t)xPortGetFreeHeapSize());
                uart_printf(UART_PORT1, "  UART1 TX free: %lu bytes\r\n", uart_tx_free_space(UART_PORT1));
                uart_printf(UART_PORT1, "  UART1 RX avail: %lu bytes\r\n", uart_rx_available(UART_PORT1));
                uart_printf(UART_PORT1, "  UART1 errors:  0x%08lX\r\n", uart_get_errors(UART_PORT1));
                uart_printf(UART_PORT1, "--------------------\r\n");
            } else if (n >= 4 && buf[0] == 't' && buf[1] == 'e' && buf[2] == 's' && buf[3] == 't') {
                uart_printf(UART_PORT1, "\r\n[Test] Sending pattern via UART2 loopback...\r\n");
                const char *pattern = "Loopback test: STM32 DMA UART OK!\r\n";
                uint32_t plen = (uint32_t)strlen(pattern);
                uart_send(UART_PORT2, (const uint8_t *)pattern, plen, pdMS_TO_TICKS(1000));
                vTaskDelay(pdMS_TO_TICKS(200));

                uint8_t rx[128];
                int32_t rx_n = uart_recv(UART_PORT2, rx, sizeof(rx), pdMS_TO_TICKS(500));
                if (rx_n == (int32_t)plen && memcmp(pattern, rx, plen) == 0) {
                    uart_printf(UART_PORT1, "[Test] PASSED: echo matches (%ld bytes)\r\n", rx_n);
                } else if (rx_n > 0) {
                    uart_printf(UART_PORT1, "[Test] PARTIAL: got %ld/%lu bytes\r\n", rx_n, plen);
                } else {
                    uart_printf(UART_PORT1, "[Test] FAILED: no data received. Check PA2<->PA3 jumper.\r\n");
                }
            } else {
                /* echo back */
                uart_printf(UART_PORT1, "echo: ");
                uart_send(UART_PORT1, buf, (uint32_t)n, pdMS_TO_TICKS(100));
            }

            uart_printf(UART_PORT1, "\r\n> ");
        } else {
            /* no input → yield */
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

/* ================================================================
 * Task: Periodic Status LED / Heartbeat
 * ================================================================ */
#define HEARTBEAT_STACK_SIZE    128

static void heartbeat_task(void *param)
{
    (void)param;

    /* init PC13 LED (on many STM32F103 boards) */
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH  = (GPIOC->CRH & ~(GPIO_CRH_CNF13 | GPIO_CRH_MODE13))
                | GPIO_CRH_MODE13_1;  /* output, 2MHz push-pull */

    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        /* toggle LED every 500ms */
        GPIOC->ODR ^= GPIO_ODR_ODR13;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
    }
}

/* ================================================================
 * main()
 * ================================================================ */
int main(void)
{
    /* ---- hardware init ---- */
    SystemClock_Config();

    /* ---- UART init ---- */
    uart_init(UART_PORT1, 115200, 512, 1024);  /* console, large buffers */
    uart_init(UART_PORT2, 9600,   256, 512);   /* loopback port */

    /* ---- create tasks ---- */
    xTaskCreate(echo_task,      "echo",      ECHO_STACK_SIZE,      NULL, 2, NULL);
    xTaskCreate(heartbeat_task, "heartbeat", HEARTBEAT_STACK_SIZE, NULL, 1, NULL);

    /* ---- start scheduler ---- */
    vTaskStartScheduler();

    /* should never reach here */
    while (1);
}

/* ================================================================
 * FreeRTOS Hook: malloc failed callback
 * ================================================================ */
void vApplicationMallocFailedHook(void)
{
    /* malloc failed → trap for debugger */
    __disable_irq();
    while (1);
}

/* ================================================================
 * FreeRTOS Hook: stack overflow callback
 * ================================================================ */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;
    __disable_irq();
    while (1);
}
