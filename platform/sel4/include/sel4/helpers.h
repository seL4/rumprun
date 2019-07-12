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
#include <autoconf.h>
#include <rumprun/gen_config.h>
#include <vka/vka.h>
#include <vspace/vspace.h>
#include <sel4utils/thread.h>
#include <sel4utils/process.h>
#include <sel4utils/mapping.h>
#include <serial_server/client.h>
#include <sync/bin_sem.h>

#include <sel4platsupport/timer.h>
#include <platsupport/timer.h>

#include <sel4/kernel.h>
#include <sel4platsupport/pmem.h>
#include <rumprun/custom_simple.h>
#include <sel4runtime.h>

struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    /* virtual memory management interface */
    vspace_t vspace;
    /* initialised timer */
    ltimer_t ltimer;
    size_t rump_mapping_page_size_bits;
    seL4_Word rump_mapping_page_type;
    /* abstract interface over application init */
    simple_t simple;
    custom_simple_t custom_simple;

    vka_object_t pci_notification;
    vka_object_t spl_notification;
    sel4utils_thread_t pci_thread;
    sel4utils_thread_t stdio_thread;

    sync_bin_sem_t spl_semaphore;
    /* IO Ops */
    ps_io_ops_t io_ops;
    /* Irq Handler caps for PCI devices */
    seL4_CPtr caps[BMK_MAXINTR];
    /* Thread local storage base ptr */
    void *tls_base_ptr;
    /* Rumprun cmdline */
    // char cmdline[4096];
    /* Priority level for disabling interrupt thread */
    volatile int spldepth;
    /* Guard from preventing interrupt thread from disabling itsel4 */
    volatile int mask_the_mask;
    /* The PCI interrupt handler thread should wake up the runner thread */
    volatile bool should_wakeup;

};
typedef struct env *env_t;

extern struct env env;

static inline void arch_cpu_sched_settls(unsigned long btcb_tp)
{
    sel4runtime_set_tls_base(btcb_tp);
}

int arch_init_clocks(env_t env);
bmk_time_t arch_cpu_clock_monotonic(void);
bmk_time_t arch_cpu_clock_epochoffset(void);

void simple_init_rumprun(custom_simple_t *custom_simple, seL4_CPtr endpoint);
