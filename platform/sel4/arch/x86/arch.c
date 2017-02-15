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


static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    init_data_t *init = (init_data_t *) data;
    return init->io_port;
}

void
arch_init_simple(simple_t *simple)
{
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
}
