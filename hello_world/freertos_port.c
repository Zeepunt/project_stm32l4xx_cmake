/**
 * FreeRTOSHeap.c
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Zeepunt
 */
#include <stdint.h>

#include "stm32l4xx.h"

#include "FreeRTOS.h"
#include "task.h"

extern void xPortSysTickHandler(void);

#if (configAPPLICATION_ALLOCATED_HEAP == 1)
__attribute__((section(".freertos_heap"), aligned(8), used)) uint8_t ucHeap[configTOTAL_HEAP_SIZE];
#endif

#ifndef xPortSysTickHandler
void SysTick_Handler(void)
{
    /**
     * 需要确保 configTICK_RATE_HZ 和 HAL_TICK_FREQ_DEFAULT 一致
     *
     * configTICK_RATE_HZ = 1000
     * HAL_TICK_FREQ_DEFAULT = HAL_TICK_FREQ_1KHZ
     */
    HAL_IncTick();

    /**
     * Run / Sleep Mode: FreeRTOS tick 通过 SysTick_Handler 处理
     * Stop 1/2/3 Mode : FreeRTOS tick 通过 LPTIM-Tick 处理
     */
#if (configUSE_TICKLESS_IDLE != 2)
    #if (INCLUDE_xTaskGetSchedulerState == 1)
        if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
            xPortSysTickHandler();
        }
    #else
        xPortSysTickHandler();
    #endif
#endif
}
#else
#endif
