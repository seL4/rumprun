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

#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <sel4/helpers.h>
#include <stdio.h>
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
    return seL4_CNode_Copy(root, index, depth, init->root_cnode,
            sel4platsupport_timer_objs_get_irq_cap(&init->to, irq, PS_INTERRUPT), seL4_WordBits, seL4_AllRights);
}

static seL4_Error
get_timer_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
              UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
              UNUSED seL4_Word handle, seL4_Word vector)
{
    init_data_t *init = (init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            sel4platsupport_timer_objs_get_irq_cap(&init->to, vector, PS_MSI), seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
}

static seL4_Error
get_timer_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
                 seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    init_data_t *init = (init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            sel4platsupport_timer_objs_get_irq_cap(&init->to, pin, PS_IOAPIC), seL4_WordBits);
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
    int error = sel4platsupport_init_timer_irqs(&env->vka, &env->simple,
            env->timer_notification.cptr, &env->timer, env->custom_simple.timer_config.hw.to);
    ZF_LOGF_IF(error, "Failed to init default timer");

    if (!error) {
        /* if this succeeds, sel4test-driver has set up the hpet for us */
        ps_irq_t irq;
        error = ltimer_hpet_describe_with_region(&env->timer.ltimer, env->io_ops, env->custom_simple.timer_config.hw.to->objs[0].region, &irq);
        if (!error) {
            ZF_LOGD("Trying HPET");
            error = ltimer_hpet_init(&env->timer.ltimer, env->io_ops, irq, env->custom_simple.timer_config.hw.to->objs[0].region);
        }
    }

    if (error) {
        /* Get the PIT instead */
        ZF_LOGD("Using PIT timer");
        error = ltimer_pit_init_freq(&env->timer.ltimer, env->io_ops, simple_get_arch_info(&env->simple));
        ZF_LOGF_IF(error, "Failed to init pit");
    }
    return error;
}
