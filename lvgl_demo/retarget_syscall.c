/**
 * retarget_syscall.c
 *
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2026 Zeepunt
 */
#include <stddef.h> 
#include <stdio.h>

#include "autoconf.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "drv_uart.h"

static SemaphoreHandle_t s_uart_mutex = NULL;

static void priv_uart_mutex_lock(void)
{
    if (xPortIsInsideInterrupt() == pdTRUE) {
        return;
    }

    if (s_uart_mutex == NULL) {
        s_uart_mutex = xSemaphoreCreateRecursiveMutex();
    }

    if (s_uart_mutex != NULL) {
        xSemaphoreTakeRecursive(s_uart_mutex, portMAX_DELAY);
    }
}

static void priv_uart_mutex_unlock(void)
{
    if (xPortIsInsideInterrupt() == pdTRUE) {
        return;
    }

    if (s_uart_mutex != NULL) {
        xSemaphoreGiveRecursive(s_uart_mutex);
    }
}

#if defined (__ARMCC_VERSION) && (__ARMCC_VERSION >= 6100100)
void *malloc(size_t size)
{
    return pvPortMalloc(size);
}

void *calloc(size_t nmemb, size_t size)
{
    return pvPortCalloc(nmemb, size);
}

void free(void *ptr)
{
    vPortFree(ptr);
}

#elif defined (__GNUC__)
#include <reent.h>
#include <sys/stat.h>
#include <unistd.h>

void *__wrap_malloc(size_t size)
{
    return pvPortMalloc(size);
}

void *__wrap__malloc_r(struct _reent *reent, size_t size)
{
    (void)reent;
    return pvPortMalloc(size);
}

void *__wrap_calloc(size_t nmemb, size_t size)
{
    return pvPortCalloc(nmemb, size);
}

void *__wrap__calloc_r(struct _reent *reent, size_t nmemb, size_t size)
{
    (void)reent;
    return pvPortCalloc(nmemb, size);
}

void __wrap_free(void *ptr)
{
    vPortFree(ptr);
}

void __wrap__free_r(struct _reent *reent, void *ptr)
{
    (void)reent;
    vPortFree(ptr);
}

/* stdout */
#ifndef CONFIG_SDK_SEGGER_RTT
int _write(int file, char *ptr, int len)
{
    (void)file;

    priv_uart_mutex_lock();
    drv_uart_write((const uint8_t *)ptr, len);
    priv_uart_mutex_unlock();

    return len;
}
#endif

/* file */
int _close(int fd)
{
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st)
{ 
    (void)fd;
    (void)st;
    return -1;
}

int _isatty(int fd) 
{
    (void)fd;
    return 1;
}

int _lseek(int fd, int off, int wh)
{
    (void)fd;
    (void)off;
    (void)wh;
    return 0;
}

int _read(int fd, char *buf, int len)
{
    (void)fd;
    (void)buf;
    (void)len;
    return 0;
}
#endif
