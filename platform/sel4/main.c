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

#include <rumprun/init_data.h>
#include <simple/simple.h>
#include <utils/util.h>
#include <sel4/helpers.h>
#include <rumprun/custom_simple.h>

int init_rumprun(custom_simple_t *custom_simple);

int main(int argc, char **argv)
{

    ZF_LOGF_IF(argc != 2, "Incorrect number of arguments passed");
    seL4_CPtr endpoint = (seL4_CPtr) atoi(argv[1]);
    env.custom_simple.simple = &env.simple;
    simple_init_rumprun(&env.custom_simple, endpoint);
    return init_rumprun(&env.custom_simple);

}
