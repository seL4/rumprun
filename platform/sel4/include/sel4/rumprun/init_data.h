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
#define TIMER_LABEL 0x00000F00
#define MAX_REGIONS 4

/* These are also defined in librumpsel4_studio/sel4_stdio.h */
#define RR_STDIN 0
#define RR_STDOUT 1
#define RR_STDERR 2
#define RR_NUMIO 3
#define RR_STDIO_PAGE_BITS PAGE_BITS_4K

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

/* data shared between root task and a rumprun app.
 * all caps are in the rumprun process' cspace */
typedef struct {
#ifdef CONFIG_IOMMU
    seL4_CPtr io_space;
#endif /* CONFIG_IOMMU */
#ifdef CONFIG_ARM_SMMU
    seL4_SlotRegion io_space_caps;
#endif
    /* cap to the io ports */
    seL4_CPtr io_port;

    /* ntfn to wait on for timeouts */
    seL4_CPtr timer_signal;
    /* ep for sending RPCs to initial task */
    seL4_CPtr rpc_ep;
    /* ep for talking to the serial server thread */
    seL4_CPtr serial_ep;
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
    void *stdio[3];
    seL4_CPtr stdio_eps[3];
    /* Rump cmdline */
    char cmdline[RUMP_CONFIG_MAX];

    /* Size of memory to give to rumprun */
    size_t rumprun_memory_size;
    /* priority the process is running at */
    int priority;
    /* the number of pages in the stack */
    int stack_pages;

    /* tsc frequency */
    seL4_Word tsc_freq;
    /* address of the stack */
    void *stack;
} init_data_t;


void arch_init_simple(simple_t *simple);
