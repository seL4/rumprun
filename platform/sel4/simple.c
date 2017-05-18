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
#include <simple/simple.h>
#include <utils/util.h>
#include <sel4/helpers.h>
static int simple_default_cap_count(void *data) {
    assert(data);

    init_data_t * init_data = data;

    return   init_data->free_slots.start;
}


static seL4_CPtr simple_default_init_cap(void *data, seL4_CPtr cap_pos) {
    init_data_t *init_data = data;
    switch (cap_pos) {
        case seL4_CapNull:  /* null cap */
            return seL4_CapNull;
        break;
        case seL4_CapInitThreadTCB: /* initial thread's TCB cap */
            return init_data->tcb;
        break;
        case seL4_CapInitThreadCNode: /* initial thread's root CNode cap */
            return init_data->root_cnode;
        break;
        case seL4_CapInitThreadVSpace: /* initial thread's VSpace cap */
            return init_data->page_directory;
        break;
        case seL4_CapIRQControl: /* global IRQ controller cap */
            return init_data->irq_control;
        break;
        case seL4_CapASIDControl: /* global ASID controller cap */
            ZF_LOGF("No ASIDControl cap provided");
            return seL4_CapNull;
        break;
        case seL4_CapInitThreadASIDPool: /* initial thread's ASID pool cap */
            ZF_LOGF("No InitThreadASIDPool cap provided");
            return seL4_CapNull;
        break;
        case seL4_CapIOPort: /* global IO port cap (null cap if not supported) */
            return init_data->io_port;
        break;
        case seL4_CapIOSpace: /* global IO space cap (null cap if no IOMMU support) */
            ZF_LOGE("This shouldn't be currently supported");
#ifdef CONFIG_IOMMU
            return init_data->io_space;
#else
            return seL4_CapNull;
#endif
        break;
        case seL4_CapBootInfoFrame: /* bootinfo frame cap */
            ZF_LOGF("No InitThreadASIDPool cap provided");
            return seL4_CapNull;
        break;
        case seL4_CapInitThreadIPCBuffer: /* initial thread's IPC buffer frame cap */
            ZF_LOGF("No InitThreadIPCBuffer cap provided");
            return seL4_CapNull;
        break;
        case seL4_CapDomain: /* global domain controller cap */
            ZF_LOGF("No Domain cap provided");
            return seL4_CapNull;
        break;
        default:
            ZF_LOGF("Invalid init cap provided");
    }
    return seL4_CapNull;
}

static uint8_t simple_default_cnode_size(void *data) {
    assert(data);

    return ((init_data_t *)data)->cspace_size_bits;
}

static int simple_default_untyped_count(void *data) {
    assert(data);

    return ((init_data_t *)data)->untypeds.end - ((init_data_t *)data)->untypeds.start;
}

static seL4_CPtr simple_default_nth_untyped(void *data, int n, size_t *size_bits, uintptr_t *paddr, uint8_t *device) {
    assert(data && size_bits && paddr);

    init_data_t *init_data = data;

    if(n < (init_data->untypeds.end - init_data->untypeds.start)) {
        if(paddr != NULL) {
            *paddr = init_data->untyped_list[n].untyped_paddr;
        }
        if(size_bits != NULL) {
            *size_bits = init_data->untyped_list[n].untyped_size_bits;
        }
        if (device != NULL) {
            *device = (uint8_t)init_data->untyped_list[n].untyped_is_device;
        }
        return init_data->untypeds.start + (n);
    }

    return seL4_CapNull;
}




static seL4_Word simple_default_arch_info(void *data) {
    ZF_LOGE_IF(data == NULL, "Data is null!");

    return ((init_data_t *)data)->tsc_freq;
}

int custom_simple_vspace_bootstrap_frames(simple_t *simple, vspace_t *vspace, sel4utils_alloc_data_t *alloc_data,
                            vka_t *vka) {
    init_data_t *init_data = simple->data;
    void *existing_frames[init_data->stack_pages + 4];
    existing_frames[0] = (void *) init_data;
    existing_frames[1] = ((char *) init_data) + PAGE_SIZE_4K;
    existing_frames[2] = seL4_GetIPCBuffer();
    ZF_LOGF_IF(init_data->stack_pages == 0, "No stack");
    int i;
    for (i = 0; i < init_data->stack_pages; i++) {
        existing_frames[i + 3] = init_data->stack + (i * PAGE_SIZE_4K);
    }
    existing_frames[i + 3] = NULL;
    return sel4utils_bootstrap_vspace(vspace, alloc_data, simple_get_pd(simple), vka,
                                       NULL, NULL, existing_frames);

}

int custom_get_priority(simple_t *simple) {
    init_data_t *init_data = simple->data;
    return init_data->priority;
}

char *custom_get_cmdline(simple_t *simple) {
    init_data_t *init_data = simple->data;
    return init_data->cmdline;

}


static init_data_t *
receive_init_data(seL4_CPtr endpoint)
{
    /* wait for a message */
    seL4_Word badge;
    UNUSED seL4_MessageInfo_t info;

    info = seL4_Recv(endpoint, &badge);

    /* check the label is correct */
    ZF_LOGF_IF(seL4_MessageInfo_get_length(info) != 1, "Incorrect Label");

    init_data_t *init_data = (init_data_t *) seL4_GetMR(0);
    ZF_LOGF_IF(init_data->free_slots.start == 0, "Bad init data");
    ZF_LOGF_IF(init_data->free_slots.end == 0, "Bad init data");

    return init_data;
}


void simple_init_rumprun(simple_t *simple, seL4_CPtr endpoint) {
    assert(simple);
    init_data_t *init_data = receive_init_data(endpoint);
    assert(init_data);

    simple->data = init_data;
    simple->cap_count = &simple_default_cap_count;
    simple->init_cap = &simple_default_init_cap;
    simple->cnode_size = &simple_default_cnode_size;
    simple->untyped_count = &simple_default_untyped_count;
    simple->nth_untyped = &simple_default_nth_untyped;
    simple->arch_info = &simple_default_arch_info;
    arch_init_simple(simple);
}
