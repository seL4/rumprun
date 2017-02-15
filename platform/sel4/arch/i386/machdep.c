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

#include <bmk-core/types.h>
#include <sel4/kernel.h>
#include <sel4/sel4.h>
#include <bmk-core/core.h>
#include <bmk-core/sched.h>
#include <bmk-core/printf.h>
#include <sel4/helpers.h>
#include <assert.h>
#include <stdio.h>


void
bmk_platform_cpu_sched_settls(struct bmk_tcb *next)
{
    env.tls_base_ptr = (void *) next->btcb_tp;
}
