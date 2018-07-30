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

#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <sel4/helpers.h>
#include <stdio.h>
#include <sel4platsupport/device.h>

/* The below 2 functions are implementations of the libsel4simple arch specific interface.
   See libsel4simple/arch_include/x86/simple/arch/simple.h for further documentation */
static seL4_Error
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port, seL4_Word root, seL4_Word dest, seL4_Word depth)
{
    init_data_t *init = (init_data_t *) data;

    return seL4_CNode_Copy(root, dest, depth, simple_get_cnode(&env.simple), init->io_port, CONFIG_WORD_SIZE, seL4_AllRights);
}

void
arch_init_simple(simple_t *simple)
{
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    simple->arch_simple.data = (void *) simple->data;
}

int arch_init_tls(env_t env, seL4_Word *tls_base)
{
    // For x86, we use a layer of indirection for the TLS base pointer.
    // This allows us to avoid having to call the kernel every single time we change
    // Rump threads.
    return seL4_TCB_SetTLSBase(simple_get_tcb(&env->simple), tls_base);
}

void
arch_cpu_sched_settls(env_t env, unsigned long btcb_tp)
{
    // Just update the pointer to point to the next TLS base
    env.tls_base_ptr = (void *) btcb_tp;
}
