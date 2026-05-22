/**
 * @file    uart_port.c
 * @brief   STM32F103ZET6 HAL port implementation
 *
 * USART / DMA channel mapping (STM32F103ZET6):
 *   USART1_TX: DMA1_Channel4, USART1_RX: DMA1_Channel5
 *   USART2_TX: DMA1_Channel7, USART2_RX: DMA1_Channel6
 *   USART3_TX: DMA1_Channel2, USART3_RX: DMA1_Channel3
 *
 * Clock: USART1 on APB2 (72 MHz), USART2/3 on APB1 (36 MHz)
 */

#include "uart_port.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ---- static port control blocks ---- */

typedef struct {
    USART_TypeDef            *usart;
    DMA_Channel_TypeDef      *dma_tx_ch;
    DMA_Channel_TypeDef      *dma_rx_ch;
    IRQn_Type                 usart_irq;
    IRQn_Type                 dma_tx_irq;
    IRQn_Type                 dma_rx_irq;
    uint32_t                  baudrate;

    /* callbacks registered by driver layer */
    uart_port_tx_tc_cb_t      tx_tc_cb;
    uart_port_tx_ht_cb_t      tx_ht_cb;
    uart_port_rx_tc_cb_t      rx_tc_cb;
    uart_port_rx_ht_cb_t      rx_ht_cb;
    uart_port_idle_cb_t       idle_cb;
    uart_port_error_cb_t      error_cb;

    /* state */
    volatile bool             tx_dma_busy;
    volatile bool             rx_dma_busy;
    uint8_t                  *rx_circ_buf;
    uint32_t                  rx_circ_len;
} uart_port_t;

static uart_port_t g_ports[UART_PORT_MAX];
static bool        g_ports_initialized = false;

/* ---- helper: get USART handle info per port ---- */

static void uart_port_get_hal_info(uint8_t port,
    USART_TypeDef **usart, DMA_Channel_TypeDef **tx_ch, DMA_Channel_TypeDef **rx_ch,
    IRQn_Type *usart_irq, IRQn_Type *tx_irq, IRQn_Type *rx_irq)
{
    switch (port) {
    case UART_PORT1:
        *usart    = USART1;
        *tx_ch    = DMA1_Channel4;
        *rx_ch    = DMA1_Channel5;
        *usart_irq = USART1_IRQn;
        *tx_irq    = DMA1_Channel4_IRQn;
        *rx_irq    = DMA1_Channel5_IRQn;
        break;
    case UART_PORT2:
        *usart    = USART2;
        *tx_ch    = DMA1_Channel7;
        *rx_ch    = DMA1_Channel6;
        *usart_irq = USART2_IRQn;
        *tx_irq    = DMA1_Channel7_IRQn;
        *rx_irq    = DMA1_Channel6_IRQn;
        break;
    case UART_PORT3:
        *usart    = USART3;
        *tx_ch    = DMA1_Channel2;
        *rx_ch    = DMA1_Channel3;
        *usart_irq = USART3_IRQn;
        *tx_irq    = DMA1_Channel2_IRQn;
        *rx_irq    = DMA1_Channel3_IRQn;
        break;
    default:
        *usart = NULL;
        *tx_ch = NULL;
        *rx_ch = NULL;
        break;
    }
}

/* ---- HAL MSP callbacks ---- */

static void uart_port_msp_init(uint8_t port)
{
    GPIO_InitTypeDef  gpio = {0};
    DMA_HandleTypeDef hdma_tx, hdma_rx;

    switch (port) {
    case UART_PORT1:
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* PA9 = TX, PA10 = RX */
        gpio.Pin       = GPIO_PIN_9;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gpio);
        gpio.Pin       = GPIO_PIN_10;
        gpio.Mode      = GPIO_MODE_INPUT;
        gpio.Pull      = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
        break;
    case UART_PORT2:
        __HAL_RCC_USART2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();
        /* PA2 = TX, PA3 = RX */
        gpio.Pin       = GPIO_PIN_2;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOA, &gpio);
        gpio.Pin       = GPIO_PIN_3;
        gpio.Mode      = GPIO_MODE_INPUT;
        gpio.Pull      = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOA, &gpio);
        break;
    case UART_PORT3:
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        /* PB10 = TX, PB11 = RX */
        gpio.Pin       = GPIO_PIN_10;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &gpio);
        gpio.Pin       = GPIO_PIN_11;
        gpio.Mode      = GPIO_MODE_INPUT;
        gpio.Pull      = GPIO_NOPULL;
        HAL_GPIO_Init(GPIOB, &gpio);
        break;
    }

    /* DMA clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();
}

/* ---- public API ---- */

void uart_port_init(uint8_t port, uint32_t baudrate)
{
    if (port >= UART_PORT_MAX) return;

    uart_port_t *p = &g_ports[port];
    memset(p, 0, sizeof(*p));

    USART_TypeDef       *usart;
    DMA_Channel_TypeDef *tx_ch, *rx_ch;
    IRQn_Type            usart_irq, tx_irq, rx_irq;

    uart_port_get_hal_info(port, &usart, &tx_ch, &rx_ch,
        &usart_irq, &tx_irq, &rx_irq);

    p->usart      = usart;
    p->dma_tx_ch  = tx_ch;
    p->dma_rx_ch  = rx_ch;
    p->usart_irq  = usart_irq;
    p->dma_tx_irq = tx_irq;
    p->dma_rx_irq = rx_irq;
    p->baudrate   = baudrate;

    uart_port_msp_init(port);

    /* ---- USART init ---- */
    uint32_t pclk;
    if (port == UART_PORT1) {
        pclk = HAL_RCC_GetPCLK2Freq();  /* APB2 = 72 MHz */
    } else {
        pclk = HAL_RCC_GetPCLK1Freq();  /* APB1 = 36 MHz */
    }

    /* Disable UART before config */
    CLEAR_BIT(usart->CR1, USART_CR1_UE);

    usart->BRR = (pclk + baudrate / 2) / baudrate;

    /* 8N1, enable TX/RX, enable IDLE interrupt */
    usart->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
    usart->CR2 = 0;
    usart->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;

    /* Disable DMA channels before config (must be done with EN=0) */
    tx_ch->CCR &= ~DMA_CCR_EN;
    rx_ch->CCR &= ~DMA_CCR_EN;

    /* TX DMA: memory-to-peripheral, read from memory, 8-bit, normal mode */
    tx_ch->CCR = DMA_CCR_MINC            /* memory increment */
               | DMA_CCR_DIR             /* mem → periph */
               | (DMA_CCR_PL_0 | DMA_CCR_PL_1) /* very high priority */
               | DMA_CCR_TCIE;           /* TC interrupt */

    /* RX DMA: peripheral-to-memory, 8-bit, CIRCULAR mode */
    rx_ch->CCR = DMA_CCR_MINC            /* memory increment */
               | DMA_CCR_CIRC            /* circular mode */
               | (DMA_CCR_PL_1)          /* high priority */
               | DMA_CCR_TCIE            /* TC interrupt */
               | DMA_CCR_HTIE;           /* half-transfer interrupt */

    /* NVIC: UART IRQ priority must be >= DMA IRQ priority to call FreeRTOS API */
    HAL_NVIC_SetPriority(usart_irq, 6, 0);
    HAL_NVIC_SetPriority(tx_irq,   7, 0);
    HAL_NVIC_SetPriority(rx_irq,   7, 0);

    HAL_NVIC_EnableIRQ(usart_irq);
    /* DMA IRQs enabled on demand */

    /* Enable UART */
    SET_BIT(usart->CR1, USART_CR1_UE);

    g_ports_initialized = true;
}

void uart_port_deinit(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];

    /* disable DMA channels */
    p->dma_tx_ch->CCR &= ~DMA_CCR_EN;
    p->dma_rx_ch->CCR &= ~DMA_CCR_EN;

    /* disable UART */
    CLEAR_BIT(p->usart->CR1, USART_CR1_UE);

    HAL_NVIC_DisableIRQ(p->usart_irq);
    HAL_NVIC_DisableIRQ(p->dma_tx_irq);
    HAL_NVIC_DisableIRQ(p->dma_rx_irq);
}

/* ---- DMA operations ---- */

void uart_port_dma_tx_start(uint8_t port, const uint8_t *data, uint32_t len)
{
    if (port >= UART_PORT_MAX || !data || len == 0) return;
    uart_port_t *p = &g_ports[port];

    /* ensure previous TX DMA is stopped */
    p->dma_tx_ch->CCR &= ~DMA_CCR_EN;

    p->dma_tx_ch->CPAR   = (uint32_t)&p->usart->DR;
    p->dma_tx_ch->CMAR   = (uint32_t)data;
    p->dma_tx_ch->CNDTR  = (uint16_t)len;

    p->tx_dma_busy = true;
    HAL_NVIC_EnableIRQ(p->dma_tx_irq);

    p->dma_tx_ch->CCR |= DMA_CCR_EN;
}

void uart_port_dma_rx_start(uint8_t port, uint8_t *buf, uint32_t len)
{
    if (port >= UART_PORT_MAX || !buf || len == 0) return;
    uart_port_t *p = &g_ports[port];

    p->dma_rx_ch->CCR &= ~DMA_CCR_EN;

    p->dma_rx_ch->CPAR   = (uint32_t)&p->usart->DR;
    p->dma_rx_ch->CMAR   = (uint32_t)buf;
    p->dma_rx_ch->CNDTR  = (uint16_t)len;

    p->rx_circ_buf  = buf;
    p->rx_circ_len  = len;
    p->rx_dma_busy  = true;

    HAL_NVIC_EnableIRQ(p->dma_rx_irq);

    p->dma_rx_ch->CCR |= DMA_CCR_EN;
}

void uart_port_dma_tx_stop(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];
    p->dma_tx_ch->CCR &= ~DMA_CCR_EN;
    HAL_NVIC_DisableIRQ(p->dma_tx_irq);
    p->tx_dma_busy = false;
}

void uart_port_dma_rx_stop(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];
    p->dma_rx_ch->CCR &= ~DMA_CCR_EN;
    HAL_NVIC_DisableIRQ(p->dma_rx_irq);
    p->rx_dma_busy = false;
}

/* ---- DMA status ---- */

uint32_t uart_port_tx_dma_remaining(uint8_t port)
{
    if (port >= UART_PORT_MAX) return 0;
    return g_ports[port].dma_tx_ch->CNDTR;
}

uint32_t uart_port_rx_dma_remaining(uint8_t port)
{
    if (port >= UART_PORT_MAX) return 0;
    return g_ports[port].dma_rx_ch->CNDTR;
}

bool uart_port_dma_tx_busy(uint8_t port)
{
    if (port >= UART_PORT_MAX) return false;
    return g_ports[port].tx_dma_busy;
}

bool uart_port_dma_rx_busy(uint8_t port)
{
    if (port >= UART_PORT_MAX) return false;
    return g_ports[port].rx_dma_busy;
}

/* ---- flow control ---- */

void uart_port_rts_set(uint8_t port, bool assert)
{
    (void)port; (void)assert;
    /* STM32F103 RTS is auto-managed by USART_CR3_RTSE if enabled */
}

bool uart_port_cts_read(uint8_t port)
{
    if (port >= UART_PORT_MAX) return false;
    return (g_ports[port].usart->SR & USART_SR_CTS) != 0;
}

/* ---- callback registration ---- */

void uart_port_set_tx_tc_cb(uint8_t port, uart_port_tx_tc_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].tx_tc_cb = cb;
}

void uart_port_set_tx_ht_cb(uint8_t port, uart_port_tx_ht_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].tx_ht_cb = cb;
}

void uart_port_set_rx_tc_cb(uint8_t port, uart_port_rx_tc_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].rx_tc_cb = cb;
}

void uart_port_set_rx_ht_cb(uint8_t port, uart_port_rx_ht_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].rx_ht_cb = cb;
}

void uart_port_set_idle_cb(uint8_t port, uart_port_idle_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].idle_cb = cb;
}

void uart_port_set_error_cb(uint8_t port, uart_port_error_cb_t cb)
{
    if (port < UART_PORT_MAX) g_ports[port].error_cb = cb;
}

/* ---- interrupt handlers ---- */

void uart_port_irq_handler(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];
    uint32_t sr = p->usart->SR;

    /* ---- IDLE interrupt ---- */
    if ((sr & USART_SR_IDLE) && (p->usart->CR1 & USART_CR1_IDLEIE)) {
        /* read SR then DR to clear IDLE flag */
        volatile uint32_t dummy = p->usart->DR;
        (void)dummy;
        if (p->idle_cb) {
            p->idle_cb(port);
        }
    }

    /* ---- error handling ---- */
    uint32_t error_flags = sr & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE);
    if (error_flags) {
        /* clear error flags by reading SR then DR */
        volatile uint32_t dr = p->usart->DR;
        (void)dr;
        if (p->error_cb) {
            p->error_cb(port, error_flags);
        }
    }

    /* ---- RXNE (fallback: byte-by-byte if DMA disabled) ---- */
    if ((sr & USART_SR_RXNE) && !(p->usart->CR3 & USART_CR3_DMAR)) {
        volatile uint32_t dr = p->usart->DR;
        (void)dr;
    }
}

void uart_port_dma_tx_irq_handler(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];
    uint32_t isr = DMA1->ISR;
    uint32_t ch_idx; /* which channel triggered? */

    /* Determine which channel based on port */
    switch (port) {
    case UART_PORT1: ch_idx = 4; break;
    case UART_PORT2: ch_idx = 7; break;
    case UART_PORT3: ch_idx = 2; break;
    default: return;
    }

    /* Check TC flag; position depends on channel index (1-7) */
    uint32_t tc_mask = DMA_ISR_TCIF1 << ((ch_idx - 1) * 4);

    if (isr & tc_mask) {
        /* clear TC flag */
        DMA1->IFCR = tc_mask;

        p->tx_dma_busy = false;
        HAL_NVIC_DisableIRQ(p->dma_tx_irq);

        if (p->tx_tc_cb) {
            p->tx_tc_cb(port);
        }
    }
}

void uart_port_dma_rx_irq_handler(uint8_t port)
{
    if (port >= UART_PORT_MAX) return;
    uart_port_t *p = &g_ports[port];
    uint32_t isr = DMA1->ISR;
    uint32_t ch_idx;

    switch (port) {
    case UART_PORT1: ch_idx = 5; break;
    case UART_PORT2: ch_idx = 6; break;
    case UART_PORT3: ch_idx = 3; break;
    default: return;
    }

    uint32_t tc_mask  = DMA_ISR_TCIF1  << ((ch_idx - 1) * 4);
    uint32_t ht_mask  = DMA_ISR_HTIF1  << ((ch_idx - 1) * 4);

    if (isr & tc_mask) {
        DMA1->IFCR = tc_mask;
        if (p->rx_tc_cb) {
            p->rx_tc_cb(port);
        }
    }

    if (isr & ht_mask) {
        DMA1->IFCR = ht_mask;
        if (p->rx_ht_cb) {
            p->rx_ht_cb(port);
        }
    }
}
