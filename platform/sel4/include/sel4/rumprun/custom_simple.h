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
#include <rumprun/init_data.h>

enum serial_variant {
    SERIAL_SERVER,
    SERIAL_INTERFACE,
};

enum timer_variant {
    TIMER_LTIMER,
    TIMER_INTERFACE,
};

enum pci_config_variant {
    PCI_CONFIG_HW,
    PCI_CONFIG_INTERFACE
};


typedef struct serial_config {
    enum serial_variant serial;
    void (*putchar)(int c);
    seL4_CPtr ep;
} serial_config_t;

typedef struct timer_config {
    enum timer_variant timer;
    seL4_CPtr timer_ntfn;
    uint64_t tsc_freq;
    union {
        struct {
            int (*oneshot_relative)(int tid, uint64_t ns);
            uint64_t (*time)(void);
        } interface;
        struct {
            ltimer_t ltimer;
        } ltimer;
    };
} timer_config_t;

typedef struct pci_config_config {
    enum pci_config_variant pci_config;
    int32_t (*pci_config_read32)(uint8_t bus,  uint8_t dev,  uint8_t fun,  unsigned int offset);
    void (*pci_config_write32)(uint8_t bus,  uint8_t dev,  uint8_t fun,  unsigned int offset,  uint32_t val);
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
    seL4_CPtr rpc_ep;
    void *stdio_buf[3];
    seL4_CPtr stdio_ep[3];
    void (*get_char_handler)(void);
} custom_simple_t;


static inline bool is_ltimer(custom_simple_t *custom_simple)
{
    return custom_simple->timer_config.timer == TIMER_LTIMER;
}

static inline bool is_hw_serial(custom_simple_t *custom_simple)
{
    return custom_simple->serial_config.serial == SERIAL_SERVER;
}

static inline bool is_hw_pci_config(custom_simple_t *custom_simple)
{
    return custom_simple->pci_config_config.pci_config == PCI_CONFIG_HW;
}

static inline int custom_irq_from_pci_device(custom_simple_t *custom_simple,
                                             uint32_t bus, uint32_t dev, uint32_t function, ps_irq_t *irq)
{
    if (custom_simple->camkes || irq == NULL) {
        return -1;
    }

    init_data_t *init_data = custom_simple->simple->data;
    for (int i = 0; i < MAX_NUM_PCI_DEVICES; i++) {
        if (bus == init_data->interrupt_list[i].bus &&
            dev == init_data->interrupt_list[i].dev &&
            function == init_data->interrupt_list[i].function) {
            *irq = init_data->interrupt_list[i].irq;
            return 0;
        }
    }
    return -1;

}

int custom_simple_vspace_bootstrap_frames(custom_simple_t *custom_simple, vspace_t *vspace,
                                          sel4utils_alloc_data_t *alloc_data,
                                          vka_t *vka);
int custom_get_num_regions(custom_simple_t *custom_simple);
int custom_get_region_list(custom_simple_t *custom_simple, int num_regions, pmem_region_t *regions);
void rump_irq_handle(int intr, int soft_intr);
void preinit_rumprun(custom_simple_t *custom_simple);
int init_rumprun(custom_simple_t *custom_simple);

int custom_get_priority(custom_simple_t *custom_simple);
const char *custom_get_cmdline(custom_simple_t *custom_simple);
