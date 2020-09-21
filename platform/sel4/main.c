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

#include <rumprun/init_data.h>
#include <simple/simple.h>
#include <utils/util.h>
#include <utils/attribute.h>
#include <sel4/helpers.h>
#include <rumprun/custom_simple.h>
#include <sel4runtime.h>
#include <muslcsys/vsyscall.h>

int init_rumprun(custom_simple_t *custom_simple);

static void CONSTRUCTOR(MUSLCSYS_WITH_VSYSCALL_PRIORITY) pre_init(void)
{
    ZF_LOGF_IF(sel4runtime_argc() != 2, "Incorrect number of arguments passed");
    seL4_CPtr endpoint = (seL4_CPtr) atoi(sel4runtime_argv()[1]);
    env.custom_simple.simple = &env.simple;
    simple_init_rumprun(&env.custom_simple, endpoint);
    preinit_rumprun(&env.custom_simple);
    printf("Rumprun app initialised");
}

int main(int argc, char **argv)
{
    return init_rumprun(&env.custom_simple);
}
