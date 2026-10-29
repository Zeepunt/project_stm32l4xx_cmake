/**
 * region.h
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Zeepunt
 */
#ifndef __REGION_H__
#define __REGION_H__

/* Flash: 512 KB */
#define __IROM_BASE    0x08000000
#define __IROM_SIZE    0x00080000

/* SRAM1: 96 KB */
#define __RAM1_BASE    0x20000000
#define __RAM1_SIZE    0x00018000
#define __RAM1_END     (__RAM1_BASE + __RAM1_SIZE)

/* SRAM1: 32 KB */
#define __RAM2_BASE    0x10000000
#define __RAM2_SIZE    0x00008000
#define __RAM2_END     (__RAM2_BASE + __RAM2_SIZE)

/* 栈提供给中断服务函数使用 (MSP), 4 KB */
#define __STACK_SIZE   0x1000

/* 堆提供给内存分配使用 (malloc), 32 KB */
#define __HEAP_SIZE    0x8000

/* 用法: uint8_t buf[16] RAM2_SECTION; */
#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6100100)
    #define RAM2_SECTION   __attribute__((section(".ram2_data"), aligned(8), used))
#elif defined (__GNUC__)
    #define RAM2_SECTION   __attribute__((section(".ram2_data"), aligned(8), used))
#else
    #define RAM2_SECTION
#endif

#endif /* __REGION_H__ */
