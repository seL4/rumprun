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

/* This file is necessary due to rump kernel symbol namespaceing */

/* These are also defined in sel4/rumprun/init_data.h */
#ifndef RR_STDIN
#define RR_STDIN 0
#define RR_STDOUT 1
#define RR_STDERR 2
#define RR_NUMIO 3
#define RR_STDIO_PAGE_BITS 12
#endif

void rumpcomp_register_handler(void (*handler)(void));
int rumpcomp_sel4_stdio_init(int stdio);
int rumpcomp_sel4_stdio_puts(int stdio, char *buf, int len);
int rumpcomp_sel4_stdio_gets(int stdio, char *buf, int len);