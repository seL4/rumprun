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

/*-
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <bmk-core/types.h>
#include <sel4/kernel.h>
#include <sel4/helpers.h>

#include <bmk-core/printf.h>

#include <stdio.h>
#include <arch_stdio.h>

void debug_putchar(int c);
static void (*vcons_putc)(int) = debug_putchar;
static void (*vcons_flush)(void) = NULL;

/* context for talking to the serial server */
static serial_client_context_t context;
/* index in the shared buffer we have buffered up to */
static ssize_t shmem_index = -1;

static void cons_flush(void)
{
    serial_server_flush(&context, shmem_index);
    shmem_index = 0;
}

void cons_putc(int c)
{
    assert(shmem_index < context.shmem_size);
    assert(shmem_index > -1);

    context.shmem[shmem_index] = c;
    shmem_index++;

    if (c == '\n' || shmem_index == context.shmem_size) {
        cons_flush();
    }
}

void debug_putchar(int c) {
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugPutChar(c);
#endif
}

static size_t cons_write(void* data, size_t count) {
    for (int i = 0; i < count; i++) {
        vcons_putc(((char*)data)[i]);
    }
    return count;
}

void cons_init(void)
{
    if (env.custom_simple.serial_config.serial != SERIAL_SERVER) {
        vcons_putc = env.custom_simple.serial_config.putchar;
        /* no flush */
    } else {
        int error = serial_server_client_connect(env.custom_simple.serial_config.ep,
                                                 &env.vka, &env.vspace, &context);
        if (!error) {
            vcons_putc = cons_putc;
            vcons_flush = cons_flush;
            shmem_index = 0;
        }
    }
    sel4muslcsys_register_stdio_write_fn(cons_write);
    bmk_printf_init(vcons_putc, vcons_flush);
}

