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
 * Copyright (c) 2014 Antti Kantee.  All Rights Reserved.
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

#include <sel4utils/thread.h>
#include <sel4/helpers.h>
#include <sel4/kernel.h>
#include <sel4runtime.h>

#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <bmk-core/sched.h>

void
bmk_platform_cpu_sched_settls(struct bmk_tcb *next)
{
    arch_cpu_sched_settls(next->btcb_tp);
}

void
bmk_platform_cpu_sched_initcurrent(
    void *tlsarea,
    struct bmk_thread *value
) {
    sel4runtime_set_tls_variable(
        (uintptr_t)tlsarea,
        bmk_current,
        value
    );
    sel4runtime_set_tls_variable(
        (uintptr_t)tlsarea,
        __sel4_ipc_buffer,
        __sel4_ipc_buffer
    );
}


/*
 * splhigh()/spl0() internally track depth
 */
unsigned long
bmk_platform_splhigh(void)
{
    /* Return immediately if called from the interrupt handler */
    if (env.mask_the_mask) {
        return 0;
    }

    // block interrupts.
    if (env.spldepth == 0) {
        sync_bin_sem_wait(&env.spl_semaphore);
    }
    env.spldepth++;
    return 0;
}

void
bmk_platform_splx(unsigned long x)
{
    if (env.mask_the_mask) {
        return;
    }
    //enable interrupts
    if (env.spldepth == 0) {
        bmk_platform_halt("out of interrupt depth!");
    }
    if (--env.spldepth == 0) {
        sync_bin_sem_post(&env.spl_semaphore);

    }
}

void NORETURN
bmk_platform_halt(const char *panicstring)
{
    if (panicstring) {
        bmk_printf("PANIC: %s\n", panicstring);
    }
    bmk_printf("All is well in the universe.\n");
    abort();
    for (;;);
}
