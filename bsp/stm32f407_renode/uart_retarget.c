/********************************************************************************
 * Copyright (c) 2025 Vinicius Tadeu Zein
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * UART retarget for STM32F407 on Renode.
 *
 * Routes printf/_write output to USART2 (0x40004400) so that Renode's
 * FileTerminal can capture test output.  Minimal polled TX -- no interrupts
 * or DMA needed for simulation.
 ********************************************************************************/

#include <stdint.h>
#include <sys/stat.h>

#define USART2_BASE 0x40004400U
#define USART_SR    (*(volatile uint32_t *)(USART2_BASE + 0x00U))
#define USART_DR    (*(volatile uint32_t *)(USART2_BASE + 0x04U))
#define USART_BRR   (*(volatile uint32_t *)(USART2_BASE + 0x08U))
#define USART_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x0CU))

#define USART_SR_TXE  (1U << 7)
#define USART_CR1_UE  (1U << 13)
#define USART_CR1_TE  (1U << 3)

static int uart_initialized = 0;

static void uart_init(void) {
    if (uart_initialized) return;

    /* Enable USART2 clock -- on Renode this is a no-op but keeps the
       register writes consistent with real hardware flow. */
    volatile uint32_t *rcc_apb1enr = (volatile uint32_t *)0x40023840U;
    *rcc_apb1enr |= (1U << 17);

    USART_BRR = 0x0683; /* ~115200 baud at 168 MHz (approximate) */
    USART_CR1 = USART_CR1_UE | USART_CR1_TE;

    uart_initialized = 1;
}

static void uart_putchar(char c) {
    while (!(USART_SR & USART_SR_TXE)) {}
    USART_DR = (uint32_t)c;
}

/* newlib _write syscall override */
int _write(int fd, const char *buf, int len) {
    (void)fd;
    uart_init();
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(buf[i]);
    }
    return len;
}

/* Minimal stubs for newlib syscalls that nosys.specs doesn't cover */
int _close(int fd)                         { (void)fd; return -1; }
int _fstat(int fd, struct stat *st)        { (void)fd; st->st_mode = S_IFCHR; return 0; }
int _isatty(int fd)                        { (void)fd; return 1; }
int _lseek(int fd, int offset, int whence) { (void)fd; (void)offset; (void)whence; return 0; }
int _read(int fd, char *buf, int len)      { (void)fd; (void)buf; (void)len; return 0; }

extern char end;
extern char __heap_limit__;
static char *heap_end = 0;

void *_sbrk(int incr) {
    if (heap_end == 0) {
        heap_end = &end;
    }
    char *new_end = heap_end + incr;
    if (new_end > &__heap_limit__) {
        return (void *)-1;
    }
    char *prev = heap_end;
    heap_end = new_end;
    return prev;
}
