/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#pragma once
#include <sel4/machine/md.h>

#ifndef _LOCORE

#include <bmk-core/types.h>

void cons_init(void);
void cons_putc(int);
void cons_puts(const char *);

int cpu_intr_init(int);

#define BMK_INTR_ROUTED 0x01
void isr(int);
void intr_init(void);
void bmk_isr_rumpkernel(int (*)(void *), void *, int, int);
extern volatile int spldepth;


#endif /* _LOCORE */

#include <bmk-core/errno.h>

#define BMK_MAXINTR	32

#define HZ 100
