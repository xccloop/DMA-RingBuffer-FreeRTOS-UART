/**
 * @file    stm32f1xx_hal_conf.h
 * @brief   STM32F1 HAL configuration for STM32F103ZET6
 *
 * Enable only the modules this project needs.
 * Place in your project include path.
 */

#ifndef STM32F1XX_HAL_CONF_H
#define STM32F1XX_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ******************** Module Configuration ******************** */

#define HAL_MODULE_ENABLED

/* Enable HAL drivers used by this project */
#define HAL_GPIO_MODULE_ENABLED     /* Port layer: pin init */
#define HAL_DMA_MODULE_ENABLED      /* Port layer: DMA operations */
#define HAL_UART_MODULE_ENABLED     /* Port layer: USART init */
#define HAL_RCC_MODULE_ENABLED      /* System: clock init */
#define HAL_CORTEX_MODULE_ENABLED   /* System: NVIC/SysTick */
#define HAL_FLASH_MODULE_ENABLED    /* System: flash wait states */
#define HAL_PWR_MODULE_ENABLED      /* System: power control */

/* Disable unused HAL drivers to reduce compile time */
// #define HAL_ADC_MODULE_ENABLED
// #define HAL_CAN_MODULE_ENABLED
// #define HAL_I2C_MODULE_ENABLED
// #define HAL_SPI_MODULE_ENABLED
// #define HAL_TIM_MODULE_ENABLED

/* ******************** Oscillator Values ******************** */

#define HSE_VALUE            ((uint32_t)8000000)   /* External 8MHz crystal */
#define HSI_VALUE            ((uint32_t)8000000)   /* Internal 8MHz RC */
#define LSE_VALUE            ((uint32_t)32768)     /* External 32.768KHz */
#define LSI_VALUE            ((uint32_t)40000)     /* Internal 40KHz */

/* ******************** HAL Tick ******************** */

#define TICK_INT_PRIORITY    ((uint32_t)0x0F)     /* SysTick priority (lowest) */

/* ******************** Assert ******************** */

#ifdef USE_FULL_ASSERT
#define assert_param(expr)  ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr)  ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32F1XX_HAL_CONF_H */
