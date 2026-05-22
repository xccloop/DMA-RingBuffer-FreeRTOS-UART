/**
 * uart_port.c — STM32F103ZET6 HAL DMA port implementation
 *
 * USART → DMA channel mapping (STM32F103):
 *   USART1_TX=DMA1_CH4  RX=DMA1_CH5    (APB2 72MHz)
 *   USART2_TX=DMA1_CH7  RX=DMA1_CH6    (APB1 36MHz)
 *   USART3_TX=DMA1_CH2  RX=DMA1_CH3    (APB1 36MHz)
 */
#include "uart_port.h"
#include "stm32f1xx_hal.h"
#include <string.h>

typedef struct {
    USART_TypeDef       *usart;
    DMA_Channel_TypeDef *dma_tx_ch, *dma_rx_ch;
    IRQn_Type            usart_irq, dma_tx_irq, dma_rx_irq;
    port_tx_tc_cb_t      tx_tc_cb;
    port_tx_ht_cb_t      tx_ht_cb;
    port_rx_tc_cb_t      rx_tc_cb;
    port_rx_ht_cb_t      rx_ht_cb;
    port_idle_cb_t       idle_cb;
    port_error_cb_t      error_cb;
    volatile bool        tx_busy;
    uint8_t             *rx_buf;
    uint32_t             rx_len;
} port_t;

static port_t g_port[UART_PORT_MAX];

/* ── Per-port GPIO/Clock init (direct register, no HAL GPIO) ── */
static void msp_init(uint8_t p)
{
    switch (p) {
    case UART_PORT1:
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN | RCC_APB2ENR_IOPAEN;
        /* PA9 = USART1_TX: alternate push-pull, 50MHz */
        GPIOA->CRH = (GPIOA->CRH & ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9))
                   | GPIO_CRH_CNF9_1 | GPIO_CRH_MODE9_0 | GPIO_CRH_MODE9_1;
        /* PA10 = USART1_RX: input floating */
        GPIOA->CRH = (GPIOA->CRH & ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10))
                   | GPIO_CRH_CNF10_0;
        break;
    case UART_PORT2:
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN; RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
        /* PA2 = USART2_TX */
        GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF2 | GPIO_CRL_MODE2))
                   | GPIO_CRL_CNF2_1 | GPIO_CRL_MODE2_0 | GPIO_CRL_MODE2_1;
        /* PA3 = USART2_RX */
        GPIOA->CRL = (GPIOA->CRL & ~(GPIO_CRL_CNF3 | GPIO_CRL_MODE3))
                   | GPIO_CRL_CNF3_0;
        break;
    case UART_PORT3:
        RCC->APB1ENR |= RCC_APB1ENR_USART3EN; RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
        /* PB10 = USART3_TX */
        GPIOB->CRH = (GPIOB->CRH & ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10))
                   | GPIO_CRH_CNF10_1 | GPIO_CRH_MODE10_0 | GPIO_CRH_MODE10_1;
        /* PB11 = USART3_RX */
        GPIOB->CRH = (GPIOB->CRH & ~(GPIO_CRH_CNF11 | GPIO_CRH_MODE11))
                   | GPIO_CRH_CNF11_0;
        break;
    }
}

/* ── Init ── */
void uart_port_init(uint8_t p, uint32_t baud)
{
    if (p >= UART_PORT_MAX) return;
    port_t *q = &g_port[p];
    memset(q, 0, sizeof(*q));

    /* Identify HW */
    switch (p) {
    case UART_PORT1:
        q->usart = USART1; q->dma_tx_ch = DMA1_Channel4; q->dma_rx_ch = DMA1_Channel5;
        q->usart_irq = USART1_IRQn; q->dma_tx_irq = DMA1_Channel4_IRQn; q->dma_rx_irq = DMA1_Channel5_IRQn;
        break;
    case UART_PORT2:
        q->usart = USART2; q->dma_tx_ch = DMA1_Channel7; q->dma_rx_ch = DMA1_Channel6;
        q->usart_irq = USART2_IRQn; q->dma_tx_irq = DMA1_Channel7_IRQn; q->dma_rx_irq = DMA1_Channel6_IRQn;
        break;
    case UART_PORT3:
        q->usart = USART3; q->dma_tx_ch = DMA1_Channel2; q->dma_rx_ch = DMA1_Channel3;
        q->usart_irq = USART3_IRQn; q->dma_tx_irq = DMA1_Channel2_IRQn; q->dma_rx_irq = DMA1_Channel3_IRQn;
        break;
    }

    msp_init(p);

    /* Baud rate */
    uint32_t pclk = (p == UART_PORT1) ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();
    q->usart->BRR = (pclk + baud / 2) / baud;

    /* USART: TX+RX, DMA triggers, IDLE interrupt */
    q->usart->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_IDLEIE;
    q->usart->CR3 = USART_CR3_DMAT | USART_CR3_DMAR;

    /* DMA TX: mem→periph, 8-bit, normal, TC interrupt */
    q->dma_tx_ch->CCR = 0;
    q->dma_tx_ch->CCR = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_PL | DMA_CCR_TCIE;

    /* DMA RX: periph→mem, 8-bit, circular, TC+HT interrupts */
    q->dma_rx_ch->CCR = 0;
    q->dma_rx_ch->CCR = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PL_1 | DMA_CCR_TCIE | DMA_CCR_HTIE;

    /* NVIC */
    HAL_NVIC_SetPriority(q->usart_irq,   6, 0);
    HAL_NVIC_SetPriority(q->dma_tx_irq,  7, 0);
    HAL_NVIC_SetPriority(q->dma_rx_irq,  7, 0);
    HAL_NVIC_EnableIRQ(q->usart_irq);

    q->usart->CR1 |= USART_CR1_UE;  /* Enable UART */
}

void uart_port_deinit(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    port_t *q = &g_port[p];
    q->dma_tx_ch->CCR &= ~DMA_CCR_EN;
    q->dma_rx_ch->CCR &= ~DMA_CCR_EN;
    q->usart->CR1 &= ~USART_CR1_UE;
    HAL_NVIC_DisableIRQ(q->usart_irq);
    HAL_NVIC_DisableIRQ(q->dma_tx_irq);
    HAL_NVIC_DisableIRQ(q->dma_rx_irq);
}

/* ── DMA TX start/stop ── */
void uart_port_dma_tx_start(uint8_t p, const uint8_t *data, uint32_t len)
{
    if (p >= UART_PORT_MAX || !data || len == 0) return;
    port_t *q = &g_port[p];
    q->dma_tx_ch->CCR &= ~DMA_CCR_EN;       /* must disable before reconfig */
    q->dma_tx_ch->CPAR  = (uint32_t)&q->usart->DR;
    q->dma_tx_ch->CMAR  = (uint32_t)data;
    q->dma_tx_ch->CNDTR = (uint16_t)len;
    q->tx_busy = true;
    HAL_NVIC_EnableIRQ(q->dma_tx_irq);
    q->dma_tx_ch->CCR |= DMA_CCR_EN;
}

void uart_port_dma_tx_stop(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    g_port[p].dma_tx_ch->CCR &= ~DMA_CCR_EN;
    HAL_NVIC_DisableIRQ(g_port[p].dma_tx_irq);
    g_port[p].tx_busy = false;
}

/* ── DMA RX start/stop ── */
void uart_port_dma_rx_start(uint8_t p, uint8_t *buf, uint32_t len)
{
    if (p >= UART_PORT_MAX || !buf || len == 0) return;
    port_t *q = &g_port[p];
    q->dma_rx_ch->CCR &= ~DMA_CCR_EN;
    q->dma_rx_ch->CPAR  = (uint32_t)&q->usart->DR;
    q->dma_rx_ch->CMAR  = (uint32_t)buf;
    q->dma_rx_ch->CNDTR = (uint16_t)len;
    q->rx_buf = buf; q->rx_len = len;
    HAL_NVIC_EnableIRQ(q->dma_rx_irq);
    q->dma_rx_ch->CCR |= DMA_CCR_EN;
}

void uart_port_dma_rx_stop(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    g_port[p].dma_rx_ch->CCR &= ~DMA_CCR_EN;
    HAL_NVIC_DisableIRQ(g_port[p].dma_rx_irq);
}

/* ── DMA status ── */
uint32_t uart_port_tx_dma_remaining(uint8_t p) {
    return (p < UART_PORT_MAX) ? g_port[p].dma_tx_ch->CNDTR : 0;
}
uint32_t uart_port_rx_dma_remaining(uint8_t p) {
    return (p < UART_PORT_MAX) ? g_port[p].dma_rx_ch->CNDTR : 0;
}
bool uart_port_dma_tx_busy(uint8_t p) {
    return (p < UART_PORT_MAX) && g_port[p].tx_busy;
}

/* ── Callback setters ── */
void uart_port_set_tx_tc_cb(uint8_t p, port_tx_tc_cb_t cb)   { if (p < UART_PORT_MAX) g_port[p].tx_tc_cb = cb; }
void uart_port_set_tx_ht_cb(uint8_t p, port_tx_ht_cb_t cb)   { if (p < UART_PORT_MAX) g_port[p].tx_ht_cb = cb; }
void uart_port_set_rx_tc_cb(uint8_t p, port_rx_tc_cb_t cb)   { if (p < UART_PORT_MAX) g_port[p].rx_tc_cb = cb; }
void uart_port_set_rx_ht_cb(uint8_t p, port_rx_ht_cb_t cb)   { if (p < UART_PORT_MAX) g_port[p].rx_ht_cb = cb; }
void uart_port_set_idle_cb(uint8_t p, port_idle_cb_t cb)     { if (p < UART_PORT_MAX) g_port[p].idle_cb = cb; }
void uart_port_set_error_cb(uint8_t p, port_error_cb_t cb)   { if (p < UART_PORT_MAX) g_port[p].error_cb = cb; }

/* ── UART ISR ── */
void uart_port_irq_handler(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    port_t *q = &g_port[p];
    uint32_t sr = q->usart->SR;

    if ((sr & USART_SR_IDLE) && (q->usart->CR1 & USART_CR1_IDLEIE)) {
        volatile uint32_t dr __attribute__((unused)) = q->usart->DR;
        if (q->idle_cb) q->idle_cb(p);
    }

    if (sr & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) {
        volatile uint32_t dr __attribute__((unused)) = q->usart->DR;
        if (q->error_cb) q->error_cb(p, sr & 0x0F);
    }
}

/* ── DMA TX ISR ── */
void uart_port_dma_tx_irq_handler(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    port_t *q = &g_port[p];
    static const uint8_t ch_idx[3] = {4, 7, 2};  /* USART1/2/3 TX ch */
    uint32_t tc = DMA_ISR_TCIF1 << ((ch_idx[p] - 1) * 4);

    if (DMA1->ISR & tc) {
        DMA1->IFCR = tc;
        q->tx_busy = false;
        HAL_NVIC_DisableIRQ(q->dma_tx_irq);
        if (q->tx_tc_cb) q->tx_tc_cb(p);
    }
}

/* ── DMA RX ISR ── */
void uart_port_dma_rx_irq_handler(uint8_t p)
{
    if (p >= UART_PORT_MAX) return;
    port_t *q = &g_port[p];
    static const uint8_t ch_idx[3] = {5, 6, 3};  /* USART1/2/3 RX ch */
    uint32_t tc = DMA_ISR_TCIF1 << ((ch_idx[p] - 1) * 4);
    uint32_t ht = DMA_ISR_HTIF1 << ((ch_idx[p] - 1) * 4);
    uint32_t isr = DMA1->ISR;

    if (isr & tc) {
        DMA1->IFCR = tc;
        if (q->rx_tc_cb) q->rx_tc_cb(p);
    }
    if (isr & ht) {
        DMA1->IFCR = ht;
        if (q->rx_ht_cb) q->rx_ht_cb(p);
    }
}
