/**
 * @file    FreeRTOSConfig.h
 * @brief   FreeRTOS config for STM32F103ZET6 (self-contained)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* SystemCoreClock is defined in system_stm32f1xx.c */
extern uint32_t SystemCoreClock;

/*-----------------------------------------------------------
 * Kernel
 *----------------------------------------------------------*/
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    16
#define configMINIMAL_STACK_SIZE                ((unsigned short)128)
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1
#define configQUEUE_REGISTRY_SIZE               8
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

/*-----------------------------------------------------------
 * Memory
 *----------------------------------------------------------*/
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   ((size_t)(15 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP        0

/*-----------------------------------------------------------
 * Hooks
 *----------------------------------------------------------*/
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1

#define configGENERATE_RUN_TIME_STATS           0

/*-----------------------------------------------------------
 * Software timer
 *----------------------------------------------------------*/
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               2
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            256

/*-----------------------------------------------------------
 * NVIC priority mapping (STM32F103: 4-bit, 0-15)
 *
 * ISRs with priority >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY
 * can call FreeRTOS FromISR functions.
 *----------------------------------------------------------*/
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY         15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY    5
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - 4))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - 4))

/*-----------------------------------------------------------
 * FreeRTOS ISR handlers
 *----------------------------------------------------------*/
#define vPortSVCHandler       SVC_Handler
#define xPortPendSVHandler    PendSV_Handler
#define xPortSysTickHandler   SysTick_Handler

/*-----------------------------------------------------------
 * Assert
 *----------------------------------------------------------*/
#define configASSERT(x) if((x)==0) { taskDISABLE_INTERRUPTS(); for(;;); }

#endif /* FREERTOS_CONFIG_H */
