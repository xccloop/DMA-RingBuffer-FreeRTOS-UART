/**
 * stm32f1xx_hal_conf.h — STM32F103ZET6 HAL configuration
 */

#ifndef STM32F1XX_HAL_CONF_H
#define STM32F1XX_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED

#define HSE_VALUE            ((uint32_t)8000000)
#define HSI_VALUE            ((uint32_t)8000000)
#define LSE_VALUE            ((uint32_t)32768)
#define LSI_VALUE            ((uint32_t)40000)

#define TICK_INT_PRIORITY    ((uint32_t)0x0F)

#ifdef USE_FULL_ASSERT
  #define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
  void assert_failed(uint8_t *file, uint32_t line);
#else
  #define assert_param(expr) ((void)0U)
#endif

#endif /* STM32F1XX_HAL_CONF_H */
