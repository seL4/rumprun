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
#include <allocman/utspace/utspace.h>
#include <rumprun/custom_simple.h>

static int simple_default_cap_count(void *data)
{
    assert(data);

    init_data_t * init_data = data;

    return   init_data->free_slots.start;
}


static seL4_CPtr simple_default_init_cap(void *data, seL4_CPtr cap_pos)
{
    seL4_CPtr cap = sel4utils_process_init_cap(data, cap_pos);
    if (cap == seL4_CapNull && cap_pos != seL4_CapNull) {
        init_data_t *init_data = data;

        switch (cap_pos) {
        case seL4_CapIRQControl: /* global IRQ controller cap */
            cap = init_data->irq_control;
            break;
        case seL4_CapIOPortControl:
        /* IO port cap (null cap if not supported)
           This cap won't be a control cap, but we use the same slot index */
            cap = init_data->io_port;
            break;
        case seL4_CapIOSpace: /* global IO space cap (null cap if no IOMMU support) */
#ifdef CONFIG_IOMMU
            cap = init_data->io_space;
#else
            ZF_LOGE("This shouldn't be currently supported");
#endif
        default:
            break;
        }
    }
    return cap;
}

static seL4_CPtr simple_default_sched_control(void *data, int core)
{
    ZF_LOGF_IF(data == NULL, "data is NULL");
    ZF_LOGF_IF(core != 0, "Only supports core of 0");

    return ((init_data_t *)data)->sched_control;
}

static uint8_t simple_default_cnode_size(void *data)
{
    assert(data);

    return ((init_data_t *)data)->cspace_size_bits;
}

static int simple_default_untyped_count(void *data)
{
    assert(data);

    return ((init_data_t *)data)->untypeds.end - ((init_data_t *)data)->untypeds.start;
}

static seL4_CPtr simple_default_nth_untyped(void *data, int n, size_t *size_bits, uintptr_t *paddr, bool *device)
{
    assert(data && size_bits && paddr);

    init_data_t *init_data = data;

    if (n < (init_data->untypeds.end - init_data->untypeds.start)) {
        if (paddr != NULL) {
            *paddr = init_data->untyped_list[n].paddr;
        }
        if (size_bits != NULL) {
            *size_bits = init_data->untyped_list[n].size_bits;
        }
        if (device != NULL) {
            uint8_t custom_device = init_data->untyped_list[n].is_device;
            *device = custom_device == ALLOCMAN_UT_KERNEL ? 0 : 1;
        }
        return init_data->untypeds.start + (n);
    }

    return seL4_CapNull;
}


static seL4_CPtr simple_default_nth_cap(void *data, int n)
{
    return n;
}

int custom_simple_vspace_bootstrap_frames(custom_simple_t *custom_simple, vspace_t *vspace, sel4utils_alloc_data_t *alloc_data,
                                          vka_t *vka)
{
    if (custom_simple->camkes) {
        void *existing_frames_camkes[] = {
            NULL
        };
        return sel4utils_bootstrap_vspace(vspace, alloc_data, simple_get_pd(custom_simple->simple), vka,
                                          NULL, NULL, existing_frames_camkes);

    }
    init_data_t *init_data = custom_simple->simple->data;
    void *existing_frames[init_data->stack_pages + RR_NUMIO + 4];
    existing_frames[0] = (void *) init_data;
    existing_frames[1] = ((char *) init_data) + PAGE_SIZE_4K;
    existing_frames[2] = seL4_GetIPCBuffer();
    ZF_LOGF_IF(init_data->stack_pages == 0, "No stack");
    for (int i = 0; i < RR_NUMIO; i++) {
        existing_frames[i+3] = init_data->stdio[i];
    }

    int frames_index = 3 + RR_NUMIO;
    for (int i = 0; i < init_data->stack_pages; i++, frames_index++) {
        existing_frames[frames_index] = init_data->stack + (i * PAGE_SIZE_4K);
    }
    existing_frames[frames_index] = NULL;
    return sel4utils_bootstrap_vspace(vspace, alloc_data, simple_get_pd(custom_simple->simple), vka,
                                      NULL, NULL, existing_frames);

}

int custom_get_priority(custom_simple_t *custom_simple)
{
    return custom_simple->priority;
}

const char *custom_get_cmdline(custom_simple_t *custom_simple)
{
    return custom_simple->cmdline;

}


static init_data_t *
receive_init_data(seL4_CPtr endpoint)
{
    /* wait for a message */
    seL4_Word badge;
    seL4_Wait(endpoint, &badge);

    init_data_t *init_data = (init_data_t *) seL4_GetMR(0);
    ZF_LOGF_IF(init_data->free_slots.start == 0, "Bad init data");
    ZF_LOGF_IF(init_data->free_slots.end == 0, "Bad init data");

    return init_data;
}

int custom_get_num_regions(custom_simple_t *custom_simple)
{
    if (custom_simple->camkes) {
        return 0;
    }
    init_data_t *init_data = custom_simple->simple->data;
    int j = 0;
    for (int i = 0; i < (init_data->untypeds.end - init_data->untypeds.start); i++) {
        uint8_t custom_device = init_data->untyped_list[i].is_device;
        if (custom_device == ALLOCMAN_UT_DEV_MEM) {
            j++;
        }

    }
    return j;

}

int custom_get_region_list(custom_simple_t *custom_simple, int num_regions, pmem_region_t *regions)
{
    if (custom_simple->camkes) {
        return 0;
    }
    init_data_t *init_data = custom_simple->simple->data;
    int j = 0;
    for (int i = 0; i < (init_data->untypeds.end - init_data->untypeds.start); i++) {
        uint8_t custom_device = init_data->untyped_list[i].is_device;
        if (custom_device == ALLOCMAN_UT_DEV_MEM) {
            pmem_region_t region = {
                .type = PMEM_TYPE_RAM,
                .base_addr = init_data->untyped_list[i].paddr,
                .length = BIT(init_data->untyped_list[i].size_bits),
            };
            regions[j] = region;
            j++;
            if (j == num_regions) {
                return j;
            }
        }

    }
    return j;
}

static int simple_default_core_count(void *data) {
    ZF_LOGF_IF(data == NULL, "Data is NULL");
    /* Currently only support one core */
    return 1;
}

void simple_init_rumprun(custom_simple_t *custom_simple, seL4_CPtr endpoint)
{
    init_data_t *init_data = receive_init_data(endpoint);
    ZF_LOGF_IF(init_data == NULL, "Failed to allocate init data");

    simple_t *simple = custom_simple->simple;
    custom_simple->camkes = false;
    custom_simple->cmdline = init_data->cmdline;
    custom_simple->priority = init_data->priority;
    custom_simple->rumprun_memory_size = init_data->rumprun_memory_size;
    custom_simple->timer_config.timer_ntfn = init_data->timer_signal;
    custom_simple->timer_config.timer = TIMER_LTIMER;
    custom_simple->timer_config.tsc_freq = init_data->tsc_freq;
    custom_simple->serial_config.serial = SERIAL_SERVER;
    custom_simple->rpc_ep = init_data->rpc_ep;
    custom_simple->serial_config.ep = init_data->serial_ep;
    for (int i = 0; i < 3; i++) {
        custom_simple->stdio_buf[i] = init_data->stdio[i];
        custom_simple->stdio_ep[i] = init_data->stdio_eps[i];
    }
    custom_simple->get_char_handler = NULL;
    simple->data = init_data;
    simple->cap_count = &simple_default_cap_count;
    simple->init_cap = &simple_default_init_cap;
    simple->cnode_size = &simple_default_cnode_size;
    simple->untyped_count = &simple_default_untyped_count;
    simple->nth_untyped = &simple_default_nth_untyped;
    simple->nth_cap = &simple_default_nth_cap;
    simple->sched_ctrl = &simple_default_sched_control;
    simple->core_count = &simple_default_core_count;
    arch_init_simple(simple);
}
