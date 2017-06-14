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
#include <bmk-core/types.h>
#include <sel4/kernel.h>
#include <assert.h>

#include <bmk-core/pgalloc.h>
#include <bmk-core/printf.h>
#include <bmk-pcpu/pcpu.h>

#include <vka/object_capops.h>

#include <platsupport/io.h>
#include <sel4/helpers.h>
#include "pci_user.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc


// #define TRACK_PCI_MAPPINGS

#ifdef TRACK_PCI_MAPPINGS
/* Currently an arbitrary number */
#define NUM_PCI_MAPPINGS 50
static void *addresses[NUM_PCI_MAPPINGS];
static uint32_t sizes[NUM_PCI_MAPPINGS];
#endif /* TRACK_PCI_MAPPINGS */

static int intrs[BMK_MAXINTR];

/* Wrappers to pass through to sel4 */
int
rumpcomp_pci_port_out(uint32_t port, int io_size, uint32_t val)
{
    return ps_io_port_out(&env.io_ops.io_port_ops, port, io_size, val);
}

int
rumpcomp_pci_port_in(uint32_t port, int io_size, uint32_t *result)
{
    return ps_io_port_in(&env.io_ops.io_port_ops, port, io_size, result);
}

int
rumpcomp_pci_intr_type(void)
{
#ifdef CONFIG_USE_MSI_ETH
    return 1; //PCI_INTR_TYPE_MSI;
#else
    return 0; //PCI_INTR_TYPE_INTX;
#endif
}

/* Don't support iospace yet */
int
rumpcomp_pci_iospace_init(void)
{
    return 0;
}

static uint32_t
makeaddr(unsigned bus, unsigned dev, unsigned fun, int reg)
{

    return (BIT(31)) | (bus << 16u) | (dev << 11u) | (fun << 8u) | (reg & 0xfc);
}

int
rumpcomp_pci_confread(unsigned bus, unsigned dev, unsigned fun, int reg,
                      unsigned int *value)
{
    uint32_t addr;
    unsigned int data;
    int res;
    addr = makeaddr(bus, dev, fun, reg);

    res = rumpcomp_pci_port_out( PCI_CONF_ADDR, 4, addr);

    if (res) {
        return res;
    }
    res = rumpcomp_pci_port_in(PCI_CONF_DATA, 4, &data);
    if (res) {
        return res;
    }
    *value = data;
    return res;
}

int
rumpcomp_pci_confwrite(unsigned bus, unsigned dev, unsigned fun, int reg,
                       unsigned int value)
{
    uint32_t addr;
    int res;

    addr = makeaddr(bus, dev, fun, reg);
    res = rumpcomp_pci_port_out(PCI_CONF_ADDR, 4, addr);

    if (res) {
        return res;
    }
    res = rumpcomp_pci_port_out( PCI_CONF_DATA, 4, value);

    if (res) {
        return res;
    }
    return 0;
}

/* map interrupts */
/* TODO Refactor this section to better support different underlying platforms.
    Implement the arch_simple interface */
int
rumpcomp_pci_irq_map(unsigned bus, unsigned device, unsigned fun,
                     int intrline, unsigned cookie)
{
    if (cookie > BMK_MAXINTR) {
        return BMK_EGENERIC;
    }

    intrs[cookie] = intrline;
    return 0;
}

/* Create interrupt and notification objects */
void *
rumpcomp_pci_irq_establish(unsigned cookie, int (*handler)(void *), void *data)
{
    if (env.caps[intrs[cookie]] == 0) {
        int error = vka_cspace_alloc(&env.vka, &env.caps[intrs[cookie]]);
        if (error != 0) {
            ZF_LOGF("Failed to allocate cslot, error %d", error);
        }
        cspacepath_t path;
        vka_cspace_make_path(&env.vka, env.caps[intrs[cookie]], &path);

#ifdef CONFIG_IRQ_IOAPIC
        /*
         *  XXX: We use a pretty static way of picking IRQ numbers here.
         *  The rumpkernel pci driver is giving us interrupt numbers for the PIC
         *  even when we are running in IOAPIC mode.  For now, we override the IRQ
         *  numbers based on compile time mappings that are hard coded below.
         *
         *  On QEMU we get irq = 11 which is the same irq in the IOAPIC.
         *  On a Dell Haswell machine we test on, when we get irq = 3,
         *      this corresponds to IOAPIC irq 20.
         */
        int irq = intrs[cookie];
        int vector = irq;
        if (irq == 3) {
            irq = 20;
        }
        int level;
        int low_polarity;
        if (irq >= 16) {
            level = 1;
            low_polarity = 1;
        } else {
            level = 0;
            low_polarity = 0;
        }
        error = seL4_IRQControl_GetIOAPIC(env.init_data->irq_control, path.root, path.capPtr, path.capDepth, 0, irq, level,
                                          low_polarity, vector);
        if (error != 0) {
            bmk_printf("Failed to get IOAPIC, error = %d\n", error);
        }
        error = seL4_IRQHandler_Ack(env.caps[intrs[cookie]]);
        if (error != 0) {
            bmk_printf("Failed to Ack IOAPIC\n");
        }

#else /* using PIC */

        error = seL4_IRQControl_Get(env.init_data->irq_control,  intrs[cookie], path.root, path.capPtr, path.capDepth);
        if (error != 0) {
            ZF_LOGF("Failed to get IRQControl\n");
        }
        error = seL4_IRQHandler_Ack(env.caps[intrs[cookie]]);
        if (error != 0) {
            ZF_LOGF("Failed to ack IRQ handler\n");
        }
#endif /* CONFIG_IRQ_IOAPIC */

        cspacepath_t path2;
        error = vka_mint_object(&env.vka, &env.pci_notification, &path2,
                                seL4_AllRights, seL4_CapData_Badge_new(1 << (intrs[cookie])));
        if (error != 0) {
            ZF_LOGF("Failed to mint notification object\n");
        }
        error = seL4_IRQHandler_SetNotification(env.caps[intrs[cookie]], path2.capPtr);
        if (error != 0) {
            ZF_LOGF("Failed to bind IRQ to notification\n");
        }
    }

    bmk_isr_rumpkernel(handler, data, intrs[cookie], BMK_INTR_ROUTED);
    return &intrs[cookie];
}



void *
rumpcomp_pci_map(unsigned long addr, unsigned long len)
{
    ZF_LOGE("MAP: %lx, %ld", addr, len);
    void *vaddr = ps_io_map(&env.io_ops.io_mapper, addr, len, 0, PS_MEM_NORMAL);
#ifdef TRACK_PCI_MAPPINGS
    int i;
    for (i = 0; i < NUM_PCI_MAPPINGS; i++) {
        if (addresses[i] == NULL) {
            addresses[i] = vaddr;
            sizes[i] = len;
            break;
        }
    }
    if (i == NUM_PCI_MAPPINGS) {
        ZF_LOGF("ERROR: Overwrote memory\n");
    }
#endif /* TRACK_PCI_MAPPINGS */
    return vaddr;
}

void
rumpcomp_pci_unmap(void *addr)
{
    uint32_t len = 0;
#ifdef TRACK_PCI_MAPPINGS
    int i;
    for (i = 0; i < NUM_PCI_MAPPINGS; i++) {
        if (addresses[i] == addr) {
            addresses[i] = NULL;
            len = sizes[i];
            break;
        }
    }
    if (i == NUM_PCI_MAPPINGS) {
        ZF_LOGF("ERROR: Cannot find entry\n");
    }
#endif /* TRACK_PCI_MAPPINGS */
    ZF_LOGD("\trumpcomp_pci_unmap: addr 0x%p len %d\n", addr, len);
    return ps_io_unmap(&env.io_ops.io_mapper, addr, len);

}
