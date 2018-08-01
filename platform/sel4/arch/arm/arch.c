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

#include <rumprun/init_data.h>
#include <sel4/helpers.h>
#include <bmk-core/sched.h>

unsigned long bmk_cpu_arm_curtcb;

void
arch_init_simple(simple_t *simple)
{
    simple->arch_simple.data = (void *) simple->data;
}

int arch_init_tls(env_t env, seL4_Word *tls_base)
{
    /* 
     * Unlike x86 where we use a layer of indirection for the TLS base pointer,
     * we have to set the TLS base pointer directly via the kernel
     */
    return seL4_TCB_SetTLSBase(simple_get_tcb(&env->simple), *tls_base);
}

void
arch_cpu_sched_settls(env_t env, unsigned long btcb_tp)
{
    /*
     * Compatibility with the 'soft' thread pointer setting (__aeabi_read_tp)
     */
    bmk_cpu_arm_curtcb = btcb_tp;

    /* 
     * For now, since gcc doesn't have an option for ARM to use a layer of indirection
     * to set the TLS pointer, we have to go through the kernel
     */
    int res = seL4_TCB_SetTLSBase(simple_get_tcb(&env->simple), (seL4_Word)btcb_tp);
    ZF_LOGF_IF(res != 0, "seL4_TCB_SetTLSBase failed");
}
