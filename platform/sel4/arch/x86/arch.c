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

#include <sel4platsupport/plat/pit.h>
#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <platsupport/timer.h>
#include <sel4/helpers.h>
#include <stdio.h>
#include <platsupport/plat/hpet.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/plat/pit.h>
#include <sel4platsupport/device.h>


static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    init_data_t *init = (init_data_t *) data;
    return init->io_port;
}


static seL4_Error
get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    init_data_t *init = (init_data_t *) data;
    if (irq != DEFAULT_TIMER_INTERRUPT) {
        ZF_LOGF("Incorrect interrupt number");
    }

    int error = seL4_CNode_Copy(root, index, depth, init->root_cnode,
                                init->timer_irq, seL4_WordBits, seL4_AllRights);
    if (error != 0) {
        ZF_LOGF("Failed to copy irq cap\n");
    }

    return error;
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    init_data_t *init = (init_data_t *) data;
    int error = seL4_CNode_Copy(root, index, depth, init->root_cnode,
            init->timer_irq, seL4_WordBits, seL4_AllRights);
    assert(error == seL4_NoError);
    return seL4_NoError;
}

static seL4_Error
get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
           seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    init_data_t *init = (init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            init->timer_irq, seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
}

void
arch_init_simple(simple_t *simple)
{
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    env.simple.arch_simple.ioapic = get_ioapic;
    env.simple.arch_simple.irq = get_irq;
    env.simple.arch_simple.msi = get_msi;
    env.simple.arch_simple.data = (void *) env.simple.data;
}


int arch_init_timer(env_t env, init_data_t *init_data) {
    /* FIXME Make this more platform agnostic */
#ifdef CONFIG_IRQ_IOAPIC
/* Map the HPET so we can query its proprties */
    vka_object_t frame;
    void *vaddr;
    uintptr_t paddr = init_data->untyped_list[init_data->timer_slot_index].untyped_paddr;
    vaddr = sel4platsupport_map_frame_at(&env->vka, &env->vspace, paddr, seL4_PageBits, &frame);
    int irq;
    int vector;
    ZF_LOGF_IF(vaddr == NULL, "Failed to map HPET paddr");
    if (!hpet_supports_fsb_delivery(vaddr)) {
        if (!config_set(CONFIG_IRQ_IOAPIC)) {
            ZF_LOGF("HPET does not support FSB delivery and we are not using the IOAPIC");
        }
        uint32_t irq_mask = hpet_ioapic_irq_delivery_mask(vaddr);
        /* grab the first irq */
        irq = FFS(irq_mask) - 1;
    } else {
        irq = -1;
    }
    vector = DEFAULT_TIMER_INTERRUPT;
    vspace_unmap_pages(&env->vspace, vaddr, 1, seL4_PageBits, VSPACE_PRESERVE);
    vka_free_object(&env->vka, &frame);
    env->timer = sel4platsupport_get_hpet_paddr(&env->vspace, &env->simple, &env->vka,
                                     paddr, env->timer_notification.cptr,
                                     irq, vector);
#else
    env->timer = sel4platsupport_get_pit(&env->vka, &env->simple, NULL, env->timer_notification.cptr);
#endif
    if (env->timer == NULL) {
        ZF_LOGF("Failed to initialise default timer");
    }
    return 0;
}
