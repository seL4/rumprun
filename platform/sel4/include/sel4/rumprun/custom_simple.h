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
#include <simple/simple.h>
#include <vspace/vspace.h>
#include <vka/vka.h>
#include <sel4platsupport/pmem.h>
#include <sel4utils/vspace.h>
#include <sel4platsupport/timer.h>

enum serial_variant {
    SERIAL_HW,
    SERIAL_INTERFACE,
};

enum timer_variant {
    TIMER_HW,
    TIMER_INTERFACE,
};

enum pci_config_variant {
    PCI_CONFIG_HW,
    PCI_CONFIG_INTERFACE
};


typedef struct serial_config {
    enum serial_variant serial;
    void (*putchar)(int c);
} serial_config_t;

typedef struct timer_config {
    enum timer_variant timer;
    union {
        struct {
            uint64_t tsc_freq;
            seL4_Word timer_cap;
            int (*oneshot_relative)(int tid, uint64_t ns);
        } interface;
        struct {
            timer_objects_t *to;
        } hw;
    };
} timer_config_t;

typedef struct pci_config_config {
    enum pci_config_variant pci_config;
    int32_t (*pci_config_read32)( uint8_t bus,  uint8_t dev,  uint8_t fun,  unsigned int offset);
    void (*pci_config_write32)( uint8_t bus,  uint8_t dev,  uint8_t fun,  unsigned int offset,  uint32_t val);
} pci_config_config_t;

typedef struct ethernet_intr_config {
    int (*eth_irq_acknowledge)(void);
} ethernet_intr_config_t;

typedef struct custom_simple {
    const char *cmdline;
    int priority;
    bool camkes;
    simple_t *simple;
    size_t rumprun_memory_size;
    serial_config_t serial_config;
    timer_config_t timer_config;
    pci_config_config_t pci_config_config;
    ethernet_intr_config_t ethernet_intr_config;
} custom_simple_t;


static inline bool is_hw_timer(custom_simple_t *custom_simple)
{
    return custom_simple->timer_config.timer == TIMER_HW;
}

static inline bool is_hw_serial(custom_simple_t *custom_simple)
{
    return custom_simple->serial_config.serial == SERIAL_HW;
}

static inline bool is_hw_pci_config(custom_simple_t *custom_simple)
{
    return custom_simple->pci_config_config.pci_config == PCI_CONFIG_HW;
}

int custom_simple_vspace_bootstrap_frames(custom_simple_t *custom_simple, vspace_t *vspace, sel4utils_alloc_data_t *alloc_data,
                                          vka_t *vka);
int custom_get_num_regions(custom_simple_t *custom_simple);
int custom_get_region_list(custom_simple_t *custom_simple, int num_regions, pmem_region_t *regions);
void rump_irq_handle(int intr);
int init_rumprun(custom_simple_t *custom_simple);

int custom_get_priority(custom_simple_t *custom_simple);
const char *custom_get_cmdline(custom_simple_t *custom_simple);
