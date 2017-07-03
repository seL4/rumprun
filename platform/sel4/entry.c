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
#include <autoconf.h>
#include <sel4/sel4.h>
#include <sel4/helpers.h>
#include <stdlib.h>
#include <sel4utils/util.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <sel4platsupport/timer.h>
#include <simple/simple_helpers.h>
#include <sel4platsupport/io.h>
#include <sel4utils/iommu_dma.h>
#include <sel4utils/page_dma.h>
#include <sel4/benchmark_utilisation_types.h>
#include <allocman/utspace/utspace.h>
#include <sel4platsupport/arch/io.h>
#include <platsupport/timer.h>
#include <bmk-core/core.h>
#include <bmk-core/pgalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/string.h>
#include <bmk-core/sched.h>
#include <bmk-core/mainthread.h>
#include <sel4platsupport/platsupport.h>
#include <bmk-core/types.h>
#include <sel4/kernel.h>
#include <sel4utils/stack.h>

/* global static memory for init */
static sel4utils_alloc_data_t alloc_data;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((PAGE_SIZE_4K) * 4000)

/* allocator static pool */
#define ALLOCATOR_STATIC_POOL_SIZE ((PAGE_SIZE_4K) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* Damn linker errors: This symbol is overrided by files_to_obj.sh it is included
    here to prevent 'undefined reference to `_cpio_archive`' linker errors */
char _cpio_archive[1];

/* Environment global data */
struct env env = {
    .spldepth = 0,
    .mask_the_mask = 0,
    .should_wakeup = 0
};

extern vspace_t *muslc_this_vspace;
extern reservation_t muslc_brk_reservation;
extern void *muslc_brk_reservation_start;
sel4utils_res_t muslc_brk_reservation_memory;

static void
init_allocator(env_t env)
{
    UNUSED int error;
    UNUSED reservation_t virtual_reservation;

    /* initialise allocator */
    allocman_t *allocator = bootstrap_use_current_1level(simple_get_cnode(&env->simple),
                                                         simple_get_cnode_size_bits(&env->simple),
                                                         simple_last_valid_cap(&env->simple) + 1,
                                                         BIT(simple_get_cnode_size_bits(&env->simple)),
                                                         ALLOCATOR_STATIC_POOL_SIZE,
                                                         allocator_mem_pool);
    ZF_LOGF_IF(allocator == NULL, "Failed to bootstrap allocator");
    allocman_make_vka(&env->vka, allocator);

    int num_regions = custom_get_num_regions(&env->custom_simple);
    pmem_region_t *regions = allocman_mspace_alloc(allocator, sizeof(pmem_region_t) * num_regions, &error);
    ZF_LOGF_IF(error, "allocman_mspace_alloc failed to allocate regions");
    error = custom_get_region_list(&env->custom_simple, num_regions, regions);
    ZF_LOGF_IF(num_regions != error, "calloc returned NULL");
    allocman_add_simple_untypeds_with_regions(allocator, &env->simple, num_regions, regions);
    allocman_mspace_free(allocator, regions, sizeof(pmem_region_t) * num_regions);

    error = custom_simple_vspace_bootstrap_frames(&env->custom_simple, &env->vspace, &alloc_data, &env->vka);

    error = sel4utils_reserve_range_no_alloc(&env->vspace, &muslc_brk_reservation_memory, 1048576, seL4_AllRights, 1, &muslc_brk_reservation_start);
    ZF_LOGF_IF(error, "Failed to reserve_range");
    muslc_this_vspace = &env->vspace;
    muslc_brk_reservation.res = &muslc_brk_reservation_memory;


    /* switch the allocator to a virtual memory pool */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                               seL4_AllRights, 1, &vaddr);
    ZF_LOGF_IF(virtual_reservation.res == 0, "Failed to switch allocator to virtual memory pool");

    bootstrap_configure_virtual_pool(allocator, vaddr, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                     simple_get_pd(&env->simple));

}



static void init_timer(env_t env)
{
    int error;
    error = vka_alloc_notification(&env->vka, &env->timer_notification);
    ZF_LOGF_IF(error != 0, "Failed to allocate notification object");

    error = arch_init_timer(env);
    ZF_LOGF_IF(error != 0, "arch_init_timer failed");
}

static void
provide_vmem(env_t env)
{
    void *osend;

    bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER);

    vspace_new_pages_config_t config;
    size_t rumprun_size = env->custom_simple.rumprun_memory_size;
    size_t rumprun_pages = rumprun_size / PAGE_SIZE_4K;
    printf("num pages %zd\n", rumprun_pages);
    if (default_vspace_new_pages_config(rumprun_pages, seL4_PageBits, &config)) {
        ZF_LOGF("Failed to create config");
    }
    if (vspace_new_pages_config_use_device_ut(true, &config)) {
        ZF_LOGF("Failed to set device_ram");
    }

    osend = vspace_new_pages_with_config(&env->vspace, &config, seL4_CapRights_new(1, 1, 1));
    ZF_LOGF_IF(osend == NULL, "vspace returned null");

    printf("Starting paddr: %p\n", osend);
    bmk_pgalloc_loadmem((uintptr_t) osend, (uintptr_t) osend + rumprun_size);

    bmk_memsize = rumprun_size;
}

static void
wait_for_timer_interrupt(void * UNUSED _a, void * UNUSED _b, void * UNUSED _c)
{

    while (1) {
        seL4_Word sender_badge;
        if (is_hw_timer(&env.custom_simple)) {
            seL4_Wait(env.timer_notification.cptr, &sender_badge);
            sel4_timer_handle_single_irq(env.timer);
        } else {
            seL4_Wait(env.custom_simple.timer_config.timer_cap, &sender_badge);
        }
        seL4_Signal(env.halt_notification.cptr);
    }
}

void rump_irq_handle(int intr) {
    sync_bin_sem_wait(&env.spl_semaphore);

    ZF_LOGF_IF(env.spldepth != 0, "spldepth should be 0.  This thread should be blocked.");
    if (env.should_wakeup != 0) {
        seL4_Signal(env.halt_notification.cptr);
    }

    env.mask_the_mask = 1;

    isr(intr);
    sync_bin_sem_post(&env.spl_semaphore);
    env.mask_the_mask = 0;

}

static void wait_for_pci_interrupt(void * UNUSED _a, void * UNUSED _b, void * UNUSED _c)
{

    /* Set up TLS for main thread.  The main thread can't do this itself
        so this thread is used before it actually handles PCI stuff */
    seL4_UserContext context;
    int res = seL4_TCB_ReadRegisters(simple_get_tcb(&env.simple), 1, 0, (sizeof(seL4_UserContext) / sizeof(seL4_Word)), &context );
    ZF_LOGF_IF(res, "Could not read registers");
    context.tls_base = (seL4_Word)&env.tls_base_ptr;
    /* When you call write registers on a thread, seL4 changes it to be restarted.
       Because the target thread is of a lower priority, its last instruction was an
       invocation that set the current thread to running. The eip needs to be incremented
       so that the target thread is resumed correctly instead of recalling the invocation. */
    seL4_Word pc = sel4utils_get_instruction_pointer(context);
    sel4utils_set_instruction_pointer(&context, pc + ARCH_SYSCALL_INSTRUCTION_SIZE);
    res = seL4_TCB_WriteRegisters(simple_get_tcb(&env.simple), 1, 0, (sizeof(seL4_UserContext) / sizeof(seL4_Word)), &context );
    ZF_LOGF_IF(res, "Could not write registers");

    env.mask_the_mask = 0;
    while (1) {
        seL4_Word sender_badge;
        seL4_MessageInfo_t UNUSED mess = seL4_Recv(env.pci_notification.cptr, &sender_badge);
        rump_irq_handle(sender_badge);
    }
}

int init_rumprun(custom_simple_t *custom_simple)
{
    if (custom_simple != &env.custom_simple) {
        env.custom_simple = *custom_simple;
    }
    if (&env.simple != env.custom_simple.simple) {
        env.simple = *env.custom_simple.simple;
    }

    /* initialse cspace, vspace and untyped memory allocation */
    init_allocator(&env);

    int res;
    /* initialise the timer */
    if (env.custom_simple.timer_config.timer == TIMER_HW) {
        init_timer(&env);
    }

    /* initialise serial
        prints before here _may_ crash the system */
    if (env.custom_simple.serial_config.serial == SERIAL_HW) {
        platsupport_serial_setup_simple(NULL, &env.simple, &env.vka);
    }

    res = vka_alloc_notification(&env.vka, &env.pci_notification);
    ZF_LOGF_IF(res != 0, "Failed to allocate notification object");
    res = vka_alloc_notification(&env.vka, &env.halt_notification);
    ZF_LOGF_IF(res != 0, "Failed to allocate notification object");
    res = vka_alloc_notification(&env.vka, &env.spl_notification);
    ZF_LOGF_IF(res != 0, "Failed to allocate notification object");
    sync_bin_sem_init(&env.spl_semaphore, env.spl_notification.cptr, 1);
    sync_bin_sem_init(&env.halt_semaphore, env.halt_notification.cptr, 1);

    res = sel4utils_configure_thread(&env.vka, &env.vspace, &env.vspace, seL4_CapNull,
                                     custom_get_priority(&env.custom_simple), simple_get_cnode(&env.simple), seL4_NilData,
                                     &env.timing_thread);
    ZF_LOGF_IF(res != 0, "Configure thread failed");

    res = sel4utils_configure_thread(&env.vka, &env.vspace, &env.vspace, seL4_CapNull,
                                     custom_get_priority(&env.custom_simple), simple_get_cnode(&env.simple), seL4_NilData,
                                     &env.pci_thread);
    ZF_LOGF_IF(res != 0, "Configure thread failed");


    res = seL4_TCB_SetPriority(simple_get_tcb(&env.simple), custom_get_priority(&env.custom_simple) - 1);
    ZF_LOGF_IF(res != 0, "seL4_TCB_SetPriority thread failed");
    res = sel4utils_start_thread(&env.timing_thread, wait_for_timer_interrupt, NULL, NULL,
                                 1);

    ZF_LOGF_IF(res != 0, "sel4utils_start_thread(wait_for_timer_interrupt) failed");
    res = sel4utils_start_thread(&env.pci_thread, wait_for_pci_interrupt, NULL, NULL,
                                 1);

    ZF_LOGF_IF(res != 0, "sel4utils_start_thread(wait_for_pci_interrupt) failed");

    res = sel4platsupport_new_io_ops(env.vspace, env.vka, &env.io_ops);
    ZF_LOGF_IF(res != 0, "sel4platsupport_new_io_ops failed");

    res = sel4platsupport_get_io_port_ops(&env.io_ops.io_port_ops, &env.simple);
    ZF_LOGF_IF(res != 0, "sel4platsupport_get_io_port_ops failed");

#ifdef CONFIG_IOMMU
    seL4_CPtr io_space = simple_init_cap(&env.simple, seL4_CapIOSpace);
    res = sel4utils_make_iommu_dma_alloc(&env.vka, &env.vspace, &env.io_ops.dma_manager, 1, &io_space);
    ZF_LOGF_IF(res != 0, "sel4utils_make_iommu_dma_alloc failed");
#else
    res = sel4utils_new_page_dma_alloc(&env.vka, &env.vspace, &env.io_ops.dma_manager);
    ZF_LOGF_IF(res != 0, "sel4utils_new_page_dma_alloc failed");
#endif

    cons_init();
    bmk_printf("rump kernel bare metal bootstrap\n\n");
    x86_initclocks();

    bmk_sched_init();
    provide_vmem(&env);
    intr_init();

    bmk_sched_startmain(bmk_mainthread, (void *) custom_get_cmdline(&env.custom_simple));

    return 0;
}
