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
#include <sel4/bootinfo.h>
#include <vka/vka.h>
#include <vka/object.h>
#include <sel4utils/elf.h>
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <sel4utils/vspace.h>
#include <sel4platsupport/timer.h>

/* max test name size */
#define APP_NAME_MAX 20
#define RUMP_CONFIG_MAX 600

#define MAX_NUM_PCI_DEVICES 1
#define MAX_REGIONS 4

typedef struct {
    uint8_t size_bits;
    /* ALLOCMAN_UT_KERNEL, ALLOCMAN_UT_DEV, ALLOCMAN_UT_DEV_MEM */
    uint8_t is_device;
    uintptr_t paddr;
} untyped_descriptor_t;

typedef struct {
    uint32_t bus;
    uint32_t dev;
    uint32_t function;
    ps_irq_t irq;
} interrupt_descriptor_t;

/* data shared between root task and the rumprun app.
 * all caps are in the rumprun process' cspace */
typedef struct {
    /* page directory of the test process */
    seL4_CPtr page_directory;
    /* root cnode of the test process */
    seL4_CPtr root_cnode;
    /* tcb of the test process */
    seL4_CPtr tcb;
    /* the domain cap */
    seL4_CPtr domain;
    /* asid pool cap for the test process to use when creating new processes */
    seL4_CPtr asid_pool;
    seL4_CPtr asid_ctrl;
#ifdef CONFIG_IOMMU
    seL4_CPtr io_space;
#endif /* CONFIG_IOMMU */
#ifdef CONFIG_ARM_SMMU
    seL4_SlotRegion io_space_caps;
#endif
    /* cap to the io ports */
    seL4_CPtr io_port;
    /* objects for the default timer */
    timer_objects_t to;
    /* size of the rumprun process' cspace */
    seL4_Word cspace_size_bits;
    /* range of free slots in the cspace */
    seL4_SlotRegion free_slots;
    seL4_CPtr irq_control;
    seL4_CPtr sched_control;

    /* range of untyped memory in the cspace */
    seL4_SlotRegion untypeds;
    untyped_descriptor_t untyped_list[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];
    interrupt_descriptor_t interrupt_list[MAX_NUM_PCI_DEVICES];

    /* Rump cmdline */
    char cmdline[RUMP_CONFIG_MAX];

    /* Size of memory to give to rumprun */
    size_t rumprun_memory_size;
    /* priority the test process is running at */
    int priority;

    /* List of elf regions in the test process image, this
     * is provided so the test process can launch copies of itself.
     *
     * Note: copies should not rely on state from the current process
     * or the image. Only use copies to run code functions, pass all
     * required state as arguments. */
    sel4utils_elf_region_t elf_regions[MAX_REGIONS];

    /* the number of elf regions */
    int num_elf_regions;

    /* the number of pages in the stack */
    int stack_pages;

    /* tsc frequency */
    seL4_Word tsc_freq;
    /* address of the stack */
    void *stack;
} init_data_t;


void arch_init_simple(simple_t *simple);
