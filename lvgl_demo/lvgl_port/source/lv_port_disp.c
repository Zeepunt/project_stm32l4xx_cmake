/*
 * lv_port_disp.c
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Zeepunt
 */
#include <stdint.h>
#include <stdio.h>

#include "region.h"

#include "dev_st7789v2.h"

#include "lvgl.h"
#include "lv_port.h"

#define DISP_HOR_RES         240                                               /* unit: pixels */
#define DISP_VER_RES         240                                               /* unit: pixels */
#define DISP_COLOR           16                                                /* RGB565, unit: bits */
#define DISP_BUF_FULL_SIZE   (DISP_HOR_RES * DISP_VER_RES * (DISP_COLOR / 8))  /* 240 * 240 * (16 / 8) = 115200 Bytes */

#define DISP_BUF_PART_SIZE   (DISP_BUF_FULL_SIZE / 8)                          /* 115200 / 8 = 14400 Bytes */

static lv_display_t *s_disp = NULL;

static uint8_t s_disp_buf_1[DISP_BUF_PART_SIZE] RAM2_SECTION;
static uint8_t s_disp_buf_2[DISP_BUF_PART_SIZE] RAM2_SECTION;

static void priv_display_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_buf)
{
    uint16_t width = area->x2 - area->x1 + 1;
    uint16_t height = area->y2 - area->y1 + 1;

    dev_st7789v2_flush(
        area->x1,
        area->y1,
        width,
        height,
        (const uint8_t *)px_buf,
        (width * height * lv_color_format_get_size(lv_display_get_color_format(disp)))
    );

    /* Indicate that the buffer is available */
    lv_display_flush_ready(disp);
}

int lv_port_disp_init(void)
{
    dev_st7789v2_init(DISP_HOR_RES, DISP_VER_RES);

    s_disp = lv_display_create(DISP_HOR_RES, DISP_VER_RES);
    if (s_disp == NULL) {
        printf("display crate failed.\n");
        return -1;
    }

    lv_display_set_buffers(
        s_disp,
        s_disp_buf_1,
        s_disp_buf_2,
        DISP_BUF_PART_SIZE,
        LV_DISPLAY_RENDER_MODE_PARTIAL
    );

    lv_display_set_flush_cb(s_disp, priv_display_flush_cb);

    return 0;
}
