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

#define _VAR_STRINGIZE(...) #__VA_ARGS__
#define VAR_STRINGIZE(...) _VAR_STRINGIZE(__VA_ARGS__)


#define RUMPRUN_META_CONNECTION(instance,platform) \
    connection seL4TimeServer instance##_timer(from instance.platform_timer, to platform.timer); \
    connection seL4RPCCall instance##_serial(from instance.platform_putchar, to platform.putchar);


#define RUMPRUN_COMPONENT_DEFINITION() \
    control; \
    uses Timer platform_timer; \
    uses PutChar platform_putchar; \
    attribute int simple_untyped24_pool = 5; \
    attribute int cnode_size_bits = 16; \
    attribute int simple = 1; \
    attribute int rump_priority = 100; \
    attribute RumprunConfig rump_config; \
    attribute int sched_ctrl = 0;

#define RUMPRUN_COMPONENT_CONFIGURATION(instance, id)

