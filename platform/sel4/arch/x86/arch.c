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

/* The below functions are implementations of the libsel4simple arch specific interface.
   See libsel4simple/arch_include/x86/simple/arch/simple.h for further documentation */
static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    init_data_t *init = (init_data_t *) data;
    return init->io_port;
}


static seL4_Error
get_timer_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    init_data_t *init = (init_data_t *) data;
    ZF_LOGF_IF(irq != DEFAULT_TIMER_INTERRUPT, "Incorrect interrupt number");

    int error = seL4_CNode_Copy(root, index, depth, init->root_cnode,
                                init->timer_irq, seL4_WordBits, seL4_AllRights);
    ZF_LOGF_IF(error != 0, "Failed to copy irq cap\n");

    return error;
}


static seL4_Error
get_timer_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
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
get_timer_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
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
    simple->arch_simple.ioapic = get_timer_ioapic;
    simple->arch_simple.irq = get_timer_irq;
    simple->arch_simple.msi = get_timer_msi;
    simple->arch_simple.data = (void *) simple->data;

}


int arch_init_timer(env_t env)
{
    /* FIXME Make this more platform agnostic */
#ifdef CONFIG_IRQ_IOAPIC
    /* Map the HPET so we can query its properties */
    vka_object_t frame;;
    // TODO fix this for when the timer isn't the last cap in untyped list.
    size_t total_untyped = simple_get_untyped_count(&env->simple);
    size_t size_bits;
    uintptr_t paddr;
    bool device;
    int irq;
    int vector;
    simple_get_nth_untyped(&env->simple, total_untyped - 1, &size_bits, &paddr, &device);
    void *vaddr = sel4platsupport_map_frame_at(&env->vka, &env->vspace, paddr, seL4_PageBits, &frame);
    ZF_LOGF_IF(vaddr == NULL, "Failed to map HPET paddr");
    if (!hpet_supports_fsb_delivery(vaddr)) {
        ZF_LOGF_IF(!config_set(CONFIG_IRQ_IOAPIC), "HPET does not support FSB delivery and we are not using the IOAPIC");
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
    ZF_LOGF_IF(env->timer == NULL, "Failed to initialise default timer");
    return 0;
}
