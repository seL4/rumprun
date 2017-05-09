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

#include <sel4/sel4.h>
#include <sel4/helpers.h>
#include <stdlib.h>
#include <sel4utils/util.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <sel4platsupport/timer.h>

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

/* endpoint to call back to the root task on */
static seL4_CPtr endpoint;
/* global static memory for init */
static sel4utils_alloc_data_t alloc_data;

#define NUM_PAGES_FOR_ME 20000
#define MY_VIRTUAL_MEMORY ((1 <<seL4_PageBits) * NUM_PAGES_FOR_ME )
/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 4000)

/* allocator static pool */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* Environment global data */
struct env env = {
    .spldepth = 0,
    .mask_the_mask = 0,
    .should_wakeup = 0
};


/* Damn linker errors: This symbol is overrided by files_to_obj.sh it is included
    here to prevent 'undefined reference to `_cpio_archive`' linker errors */
char _cpio_archive[1];

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
                                                         simple_get_cap_count(&env->simple),
                                                         BIT(simple_get_cnode_size_bits(&env->simple)),
                                                         ALLOCATOR_STATIC_POOL_SIZE,
                                                         allocator_mem_pool);
    if (allocator == NULL) {
        ZF_LOGF("Failed to bootstrap allocator");
    }
    allocman_make_vka(&env->vka, allocator);

    /* fill the allocator with untypeds */
    size_t total_untyped = simple_get_untyped_count(&env->simple);

    for(int i = 0; i < total_untyped; i++) {
        size_t size_bits;
        uintptr_t paddr;
        uint8_t device;
        cspacepath_t path = allocman_cspace_make_path(allocator, simple_get_nth_untyped(&env->simple, i, &size_bits, &paddr, &device));
        error = allocman_utspace_add_uts(allocator, 1, &path, &size_bits, &paddr, device);
        if (error) {
            ZF_LOGF("Failed to add untyped objects to allocator");
        }
    }

    error = custom_simple_vspace_bootstrap_frames(&env->simple, &env->vspace, &alloc_data, &env->vka);

    error = sel4utils_reserve_range_no_alloc(&env->vspace, &muslc_brk_reservation_memory, 1048576, seL4_AllRights, 1, &muslc_brk_reservation_start);
    if (error) {
        ZF_LOGF("Failed to reserve_range");
    }
    muslc_this_vspace = &env->vspace;
    muslc_brk_reservation.res = &muslc_brk_reservation_memory;


    /* switch the allocator to a virtual memory pool */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                               seL4_AllRights, 1, &vaddr);
    if (virtual_reservation.res == 0) {
        ZF_LOGF("Failed to switch allocator to virtual memory pool");
    }

    bootstrap_configure_virtual_pool(allocator, vaddr, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                     simple_get_pd(&env->simple));

}



static void init_timer(env_t env)
{

    UNUSED int error;

    // arch_init_simple(&env->simple);

    error = vka_alloc_notification(&env->vka, &env->timer_notification);
    if (error != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }

    error = arch_init_timer(env);
    if (error != 0) {
        ZF_LOGF("arch_init_timer failed");
    }

}

static void
provide_vmem(env_t env)
{
    void *osend;

    bmk_core_init(BMK_THREAD_STACK_PAGE_ORDER);

    vspace_new_pages_config_t config;
    if (default_vspace_new_pages_config(NUM_PAGES_FOR_ME, 12, &config)) {
        ZF_LOGF("Failed to create config");
    }
    if (vspace_new_pages_config_use_device_ut(true, &config)) {
        ZF_LOGF("Failed to set device_ram");
    }

    osend = vspace_new_pages_with_config(&env->vspace, &config, seL4_CapRights_new(1, 1, 1));
    if (osend == NULL) {
        ZF_LOGF("vspace returned null");
    }

    printf("Starting paddr: %p\n", osend);
    bmk_pgalloc_loadmem((uintptr_t) osend, (uintptr_t) osend + MY_VIRTUAL_MEMORY);

    bmk_memsize = MY_VIRTUAL_MEMORY;
}

static void
wait_for_timer_interrupt(void * UNUSED _a, void * UNUSED _b, void * UNUSED _c)
{

    while (1) {
        seL4_Word sender_badge;
        seL4_Wait(env.timer_notification.cptr, &sender_badge);
        sel4_timer_handle_single_irq(env.timer);
        seL4_Signal(env.halt_notification.cptr);
    }
}


static void wait_for_pci_interrupt(void * UNUSED _a, void * UNUSED _b, void * UNUSED _c)
{

    /* Set up TLS for main thread.  The main thread can't do this itself
        so this thread is used before it actually handles PCI stuff */
    seL4_UserContext context;
    int res = seL4_TCB_ReadRegisters(simple_get_tcb(&env.simple), 1, 0, (sizeof(seL4_UserContext) / sizeof(seL4_Word)), &context );
    if (res) {
        ZF_LOGF("Could not read registers");
    }
    context.tls_base = (seL4_Word)&env.tls_base_ptr;
    /* When you call write registers on a thread, seL4 changes it to be restarted.
       Because the target thread is of a lower priority, its last instruction was an
       invocation that set the current thread to running. The eip needs to be incremented
       so that the target thread is resumed correctly instead of recalling the invocation. */
    seL4_Word pc = sel4utils_get_instruction_pointer(context);
    sel4utils_set_instruction_pointer(&context, pc + ARCH_SYSCALL_INSTRUCTION_SIZE);
    res = seL4_TCB_WriteRegisters(simple_get_tcb(&env.simple), 1, 0, (sizeof(seL4_UserContext) / sizeof(seL4_Word)), &context );
    if (res) {
        ZF_LOGF("Could not write registers");
    }

    while (1) {
        seL4_Word sender_badge;
        env.mask_the_mask = 0;
        seL4_MessageInfo_t UNUSED mess = seL4_Recv(env.pci_notification.cptr, &sender_badge);
        sync_bin_sem_wait(&env.spl_semaphore);

        if (env.spldepth != 0) {
            ZF_LOGF("spldepth should be 0.  This thread should be blocked.");
        }
        if (env.should_wakeup != 0) {
            seL4_Signal(env.halt_notification.cptr);
        }

        env.mask_the_mask = 1;

        isr(sender_badge);
        sync_bin_sem_post(&env.spl_semaphore);

    }
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        ZF_LOGF("Incorrect num args");
    }
    endpoint = (seL4_CPtr) atoi(argv[1]);

    simple_init_rumprun(&env.simple, endpoint);

    /* initialse cspace, vspace and untyped memory allocation */
    init_allocator(&env);
    int res;
    /* initialise the timer */
    init_timer(&env);
    /* initialise serial
        prints before here _may_ crash the system */
    platsupport_serial_setup_simple(NULL, &env.simple, &env.vka);


    res = vka_alloc_notification(&env.vka, &env.pci_notification);
    if (res != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }
    res = vka_alloc_notification(&env.vka, &env.halt_notification);
    if (res != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }
    res = vka_alloc_notification(&env.vka, &env.spl_notification);
    if (res != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }
    sync_bin_sem_init(&env.spl_semaphore, env.spl_notification.cptr, 1);
    sync_bin_sem_init(&env.halt_semaphore, env.halt_notification.cptr, 1);

    res = sel4utils_configure_thread(&env.vka, &env.vspace, &env.vspace, seL4_CapNull,
                                     custom_get_priority(&env.simple), simple_get_cnode(&env.simple), seL4_NilData,
                                     &env.timing_thread);
    if (res != 0) {
        ZF_LOGF("Configure thread failed");
    }

    res = sel4utils_configure_thread(&env.vka, &env.vspace, &env.vspace, seL4_CapNull,
                                     custom_get_priority(&env.simple), simple_get_cnode(&env.simple), seL4_NilData,
                                     &env.pci_thread);
    if (res != 0) {
        ZF_LOGF("Configure thread failed");
    }


    res = seL4_TCB_SetPriority(simple_get_tcb(&env.simple), custom_get_priority(&env.simple) - 1);
    if (res != 0) {
        ZF_LOGF("seL4_TCB_SetPriority thread failed");
    }
    res = sel4utils_start_thread(&env.timing_thread, wait_for_timer_interrupt, NULL, NULL,
                                 1);
    if (res != 0) {
        ZF_LOGF("sel4utils_start_thread(wait_for_timer_interrupt) failed");
    }
    res = sel4utils_start_thread(&env.pci_thread, wait_for_pci_interrupt, NULL, NULL,
                                 1);
    if (res != 0) {
        ZF_LOGF("sel4utils_start_thread(wait_for_pci_interrupt) failed");
    }

    res = sel4platsupport_new_io_ops(env.vspace, env.vka, &env.io_ops);
    if (res != 0) {
        ZF_LOGF("sel4platsupport_new_io_ops failed");
    }

    res = sel4platsupport_get_io_port_ops(&env.io_ops.io_port_ops, &env.simple);
    if (res != 0) {
        ZF_LOGF("sel4platsupport_get_io_port_ops failed");
    }

#ifdef CONFIG_IOMMU
    seL4_CPtr io_space = simple_init_cap(&env.simple, seL4_CapIOSpace);
    res = sel4utils_make_iommu_dma_alloc(&env.vka, &env.vspace, &env.io_ops.dma_manager, 1, &io_space);
    if (res != 0) {
        ZF_LOGF("sel4utils_make_iommu_dma_alloc failed");
    }
#else
    res = sel4utils_new_page_dma_alloc(&env.vka, &env.vspace, &env.io_ops.dma_manager);
    if (res != 0) {
        ZF_LOGF("sel4utils_new_page_dma_alloc failed");
    }
#endif

    cons_init();
    bmk_printf("rump kernel bare metal bootstrap\n\n");
    x86_initclocks();

    bmk_sched_init();
    provide_vmem(&env);
    intr_init();

    bmk_sched_startmain(bmk_mainthread, custom_get_cmdline(&env.simple));

    return 0;
}
