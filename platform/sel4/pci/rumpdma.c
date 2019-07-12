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
#include <rumprun/gen_config.h>
#include <bmk-core/types.h>
#include <sel4/kernel.h>

#include <bmk-core/pgalloc.h>
#include <bmk-core/printf.h>
#include <bmk-core/memalloc.h>

#include <bmk-pcpu/pcpu.h>

#include <platsupport/io.h>
#include <sel4/helpers.h>
#include <vspace/vspace.h>
#include "pci_user.h"
#include <assert.h>

typedef struct a_list a_list_t;

struct a_list {
    uintptr_t vaddr;
    uintptr_t paddr;
    uintptr_t size;
};
#define LIST_LENGTH 30
a_list_t list[LIST_LENGTH];

int rumpcomp_pci_dmalloc(size_t size, size_t align,
                         unsigned long *pap, unsigned long *vap)
{
    /* alloc and pin using platsupport implementation */
    void *mem = ps_dma_alloc(&env.io_ops.dma_manager, size, align, 1, PS_MEM_NORMAL);
    if (mem == NULL) {
        return 1;
    }
    uintptr_t pmem = ps_dma_pin(&env.io_ops.dma_manager, mem, size);

    if (pmem == 0) {
        /* return error if cannot find paddr */
        return 1;
    }
    /* Add entry to our inefficient entry array */
    int i = 0;
    for (i = 0; i < LIST_LENGTH; i++) {
        if (list[i].vaddr == 0) {
            break;
        }
    }
    if (i == LIST_LENGTH) {
        ZF_LOGD("\terror: no_free entries\n");
        return 1;
    }
    list[i].vaddr = (uintptr_t) mem;
    list[i].paddr = (uintptr_t) pmem;
    list[i].size = size;
    /* Return addresses */
    *pap = (unsigned long)pmem;
    *vap = (unsigned long)mem;

    return 0;
}

/* We already mapped in with call above ds_vacookie is *vap return from
    rumpcomp_pci_dmalloc. */
int rumpcomp_pci_dmamem_map(struct rumpcomp_pci_dmaseg *dss, size_t nseg,
                            size_t totlen, void **vap)
{
    *vap = (void *)dss[0].ds_vacookie;
    return 0;
}

void rumpcomp_pci_dmafree(unsigned long mem, size_t size)
{
    /* Unpin and free from our platsupport impl */
    ps_dma_unpin(&env.io_ops.dma_manager, (void *)mem, size);

    ps_dma_free(&env.io_ops.dma_manager, (void *)mem, size);

    /* Remove from our list */
    int i;
    for (i = 0; i < LIST_LENGTH; i++) {
        if (list[i].vaddr == mem) {
            break;
        }
    }
    if (i == LIST_LENGTH) {
        bmk_printf("\terror: cannot find entry\n");
    }
    list[i].vaddr = (uintptr_t) 0;
    list[i].paddr = (uintptr_t) 0;
    list[i].size = 0;
}

unsigned long rumpcomp_pci_virt_to_mach(void *virt)
{
    /* Try and find if its from something we mapped in previously. */
    uintptr_t vin = (uintptr_t) virt;
    for (int i = 0; i < LIST_LENGTH; i++) {
        if (vin >= list[i].vaddr) {
            if (vin < (list[i].vaddr + list[i].size)) {
                return vin - list[i].vaddr + list[i].paddr;
            }
        }
    }

    /* Couldn't find above, try and find in the system allocators */
    /* This likely means that the upper levels have an mbuf that was not allocated through rumpcomp_pci_dmalloc */
    /* Apparently this behavior is fine. */
    uintptr_t paddr = (uintptr_t) vka_utspace_paddr(&env.vka, vspace_get_cookie(&env.vspace, virt),
                                                    env.rump_mapping_page_type, env.rump_mapping_page_size_bits);
    return paddr + (vin & MASK((unsigned int) env.rump_mapping_page_size_bits));
}
