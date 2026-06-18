#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "gd32c2x1.h"

#define configCPU_CLOCK_HZ                         (SystemCoreClock)
#define configTICK_RATE_HZ                         ((TickType_t)1000U)
#define configMAX_PRIORITIES                       5U
#define configMINIMAL_STACK_SIZE                   96U
#define configTOTAL_HEAP_SIZE                      (4U * 1024U)
#define configMAX_TASK_NAME_LEN                    12U
#define configUSE_PREEMPTION                       1
#define configUSE_TIME_SLICING                     1
#define configTICK_TYPE_WIDTH_IN_BITS              TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION    0

#define configUSE_TASK_NOTIFICATIONS               1
#define configUSE_MUTEXES                          0
#define configUSE_RECURSIVE_MUTEXES                0
#define configUSE_COUNTING_SEMAPHORES              0
#define configQUEUE_REGISTRY_SIZE                  0
#define configUSE_QUEUE_SETS                       0
#define configUSE_TIMERS                           0

#define configSUPPORT_DYNAMIC_ALLOCATION           1
#define configSUPPORT_STATIC_ALLOCATION            0

#define configUSE_IDLE_HOOK                        1
#define configUSE_TICK_HOOK                        0
#define configUSE_MALLOC_FAILED_HOOK               1
#define configCHECK_FOR_STACK_OVERFLOW             2

#define configGENERATE_RUN_TIME_STATS              0
#define configUSE_TRACE_FACILITY                   0
#define configUSE_STATS_FORMATTING_FUNCTIONS       0

#define configENABLE_FPU                           0
#define configENABLE_MPU                           0
#define configENABLE_TRUSTZONE                     0
#define configRUN_FREERTOS_SECURE_ONLY             0
#define configENABLE_PAC                           0

#define configPRIO_BITS                            __NVIC_PRIO_BITS
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY    3U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 2U
#define configKERNEL_INTERRUPT_PRIORITY            (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY       (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

#define INCLUDE_vTaskPrioritySet                   0
#define INCLUDE_uxTaskPriorityGet                  0
#define INCLUDE_vTaskDelete                        0
#define INCLUDE_vTaskSuspend                       0
#define INCLUDE_vTaskDelayUntil                    1
#define INCLUDE_vTaskDelay                         1
#define INCLUDE_xTaskGetSchedulerState             1
#define INCLUDE_xTaskGetCurrentTaskHandle          0
#define INCLUDE_xTaskGetIdleTaskHandle             0
#define INCLUDE_xTaskGetTickCount                  1
#define INCLUDE_xTaskAbortDelay                    0
#define INCLUDE_xQueueGetMutexHolder               0
#define INCLUDE_uxTaskGetStackHighWaterMark        0
#define INCLUDE_eTaskGetState                      0
#define INCLUDE_xTimerPendFunctionCall             0

#define configASSERT(x)                            if((x) == 0) { for(;;) {} }

#endif /* FREERTOS_CONFIG_H */
