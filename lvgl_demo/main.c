/**
 * main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Zeepunt
 */
#include <stdio.h>
#include <string.h>

#include "autoconf.h"

#include "stm32l4xx.h"

#include "board_backtrace.h"

#include "drv_uart.h"
#include "dev_icm20608.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"

#include "lpm.h"

#ifdef CONFIG_SDK_SEGGER_RTT
#include "SEGGER_RTT.h"
#endif

#include "lvgl.h"
#include "lv_port.h"

#define LOG(fmt, ...)                printf(fmt "\r\n", ##__VA_ARGS__)

/* FreeRTOS 内部处理: (size_t)uxStackDepth * sizeof(StackType_t) */
#define FREERTOS_STACK_SIZE(size)    ((size_t)size / sizeof(StackType_t))

static lv_obj_t *s_axis_obj = NULL;
static lv_obj_t *s_axis_gyro_label = NULL;
static lv_obj_t *s_axis_accel_label = NULL;

static TimerHandle_t s_axis_timer = NULL;

void SystemClock_Config(void)
{
    /**
     * HSE: 8 MHz (OSC_IN / OSC_OUT)
     * LSE: 32.768 KHz (OSC32_IN / OSC32_OUT)
     */

    HAL_StatusTypeDef status = HAL_OK;

    RCC_OscInitTypeDef osc_init = {0};
    RCC_ClkInitTypeDef clk_init = {0};

    /* Enable access to the backup domain */
    HAL_PWR_EnableBkUpAccess();

    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    /**
     * Configure the main internal regulator output voltage
     *
     * 1. PWR_REGULATOR_VOLTAGE_SCALE1
     *    typical output voltage at 1.2 V
     *    system frequency up to 80 MHz
     *
     * 2. PWR_REGULATOR_VOLTAGE_SCALE2
     *    typical output voltage at 1.0 V
     *    system frequency up to 26 MHz
     */
    status = HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    if (status != HAL_OK) {
        LOG("HAL_PWREx_ControlVoltageScaling faield: %d", status);
    }

    /**
     * Initializes the RCC Oscillators
     *
     * RCC_HSE_BYPASS -> 有源晶振 (BYPASS Clock Source)
     * RCC_HSE_ON     -> 无源晶振 (Crystal/Ceramic Resonator)
     *
     * HSE (8 MHz) -> PLLM (/1) -> 8MHz -> PLLN (x20) -> VCO = 160 MHz
     *
     * VCO (Voltage Controlled Oscillator)
     *   -> PPLR (DIV2) -> PPLCLK = 160 MHz / 2 = 80 MHz
     *   -> PPLQ (DIV2) -> PPLQ = 160 MHz / 2 = 80 MHz
     *   -> PPLP (DIV7) -> PPLP = 160 MHz / 7 = 22.857143 MHz
     */
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSE;
    osc_init.HSEState       = RCC_HSE_ON;
    osc_init.LSEState       = RCC_LSE_ON;
    osc_init.PLL.PLLState   = RCC_PLL_ON;
    osc_init.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM       = 1;
    osc_init.PLL.PLLN       = 20;
    osc_init.PLL.PLLR       = RCC_PLLR_DIV2;
    osc_init.PLL.PLLQ       = RCC_PLLQ_DIV2;
    osc_init.PLL.PLLP       = RCC_PLLP_DIV7;
    status = HAL_RCC_OscConfig(&osc_init);
    if (status != HAL_OK) {
        LOG("HAL_RCC_OscConfig faield: %d", status);
    }

    /**
     * Initializes the CPU, AHB and APB buses clocks
     *
     * SYSCLK = PPLCLK = 80 MHz
     * HCLK   = SYSCLK / (AHBCLKDivider) = 80 MHz / 1 = 80 MHz
     * PCLK1  = HCLK / (APB1CLKDivider) = 80 MHz / 1 = 80 MHz
     * PCLK2  = HCLK / (APB2CLKDivider) = 80 MHz / 1 = 80 MHz
     *
     * For PWR_REGULATOR_VOLTAGE_SCALE1:
     * 1. HCLK <= 16 MHz -> FLASH_LATENCY_0
     * 2. HCLK <= 32 MHz -> FLASH_LATENCY_1
     * 3. HCLK <= 48 MHz -> FLASH_LATENCY_2
     * 4. HCLK <= 64 MHz -> FLASH_LATENCY_3
     * 5. HCLK <= 80 MHz -> FLASH_LATENCY_4
     */
    clk_init.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
    clk_init.APB2CLKDivider = RCC_HCLK_DIV1;
    status = HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_4);
    if (status != HAL_OK) {
        LOG("HAL_RCC_ClockConfig faield: %d", status);
    }
}

static void priv_axis_time_cb(TimerHandle_t handler)
{
    (void)handler;

    int16_t gx = 0;
    int16_t gy = 0;
    int16_t gz = 0;

    int16_t ax = 0;
    int16_t ay = 0;
    int16_t az = 0;

    dev_icm20608_gyroscope_get(&gx, &gy, &gz);
    dev_icm20608_accelerometer_get(&ax, &ay, &az);

    lv_lock();
    lv_label_set_text_fmt(s_axis_gyro_label,  "Gyro  : %d, %d, %d", gx, gy, gz);
    lv_label_set_text_fmt(s_axis_accel_label, "Accel : %d, %d, %d", ax, ay, az);
    lv_unlock();
}

static void priv_axis_sensor_init(void)
{
    dev_icm20608_init();

    lv_lock();

    lv_obj_t *obj = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
    lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
    lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s_axis_obj = obj;

    lv_obj_t *gyro_label = lv_label_create(obj);
    lv_obj_center(gyro_label);
    s_axis_gyro_label = gyro_label;

    lv_obj_t *accel_label = lv_label_create(obj);
    lv_obj_center(accel_label);
    s_axis_accel_label = accel_label;

    lv_unlock();

    s_axis_timer = xTimerCreate(
        "axis_timer",
        pdMS_TO_TICKS(20),
        pdTRUE,
        (void *)0,
        priv_axis_time_cb
    );

    if (s_axis_timer != NULL) {
        xTimerStart(s_axis_timer, pdMS_TO_TICKS(0));
    }
}

static uint32_t priv_lvgl_tick_get_cb(void)
{
    /* Return the elapsed milliseconds since startup */
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static void priv_main_task(void *args)
{
    LOG("LVGL init");

    lv_init();
    lv_tick_set_cb(priv_lvgl_tick_get_cb);

    lv_port_disp_init();

    priv_axis_sensor_init();

    LOG("LVGL running");
    while (1) {
        uint32_t ms = lv_timer_handler();
        if (ms == LV_NO_TIMER_READY) {
            ms = 5; /* LVGL recommend at least 5ms */
        }
        lv_sleep_ms(ms);
    }
    
    LOG("stack free: %lu", uxTaskGetStackHighWaterMark(NULL));

    vTaskDelete(NULL);
}

int main(int argc, char *argv[])
{
    BaseType_t ret = pdPASS;

    HAL_Init();

#ifdef CONFIG_SDK_SEGGER_RTT
    SEGGER_RTT_Init();
#endif

    SystemClock_Config();

#if (configUSE_TICKLESS_IDLE == 1)
    HAL_DBGMCU_EnableDBGSleepMode();

    __HAL_RCC_SRAM1_CLK_SLEEP_ENABLE();

    if (__HAL_RCC_DMA1_IS_CLK_ENABLED() == 0) {
        __HAL_RCC_DMA1_CLK_ENABLE();
    }
    __HAL_RCC_DMA1_CLK_SLEEP_ENABLE();
#elif (configUSE_TICKLESS_IDLE == 2)
    HAL_DBGMCU_EnableDBGStopMode();
#endif

#ifndef CONFIG_SDK_SEGGER_RTT
    drv_uart_init();
#endif

    BOARD_BACKTRACE_INIT();

    lpm_init();

    /* printf 内部会调用 malloc, 然后 pvPortMalloc -> prvHeapInit */
    LOG("SYSCLK: %d Hz", HAL_RCC_GetSysClockFreq());
    LOG("HCLK  : %d Hz", HAL_RCC_GetHCLKFreq());
    LOG("PCLK1 : %d Hz", HAL_RCC_GetPCLK1Freq());
    LOG("PCLK2 : %d Hz", HAL_RCC_GetPCLK2Freq());
    LOG("Heap free: %d, min_ever: %d", xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize());

    ret = xTaskCreate(
        priv_main_task,
        "main",
        FREERTOS_STACK_SIZE(4096),
        NULL,
        15,
        NULL
    );

    if (ret != pdPASS) {
        LOG("xTaskCreate failed: %ld", ret);
    }

    vTaskStartScheduler();

    /* should not be here */
    while (1);

    return 0;
}
