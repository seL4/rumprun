/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <sel4/kernel.h>

#include <utils/util.h>
#include <platsupport/timer.h>
#include <bmk-core/core.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <sel4/helpers.h>
#include <stdio.h>

/* RTC wall time offset at monotonic time base. */
static bmk_time_t rtc_epochoffset;

int
arch_init_clocks(env_t env)
{
    /* 
     * For now, we are using the ltimer implementation in libplatsupport
     * and don't have a clock like the TSC on ARM
     */
    return 0;
}


bmk_time_t
arch_cpu_clock_monotonic(void)
{
    bmk_time_t now = 0;

    timer_config_t *simple_timer_config = &env.custom_simple.timer_config;
    if (simple_timer_config->timer == TIMER_INTERFACE) {
        now = simple_timer_config->interface.time();
    } else {
        UNUSED int err = ltimer_get_time(&simple_timer_config->ltimer.ltimer, (uint64_t *) &now);
    }

    return now;
}

bmk_time_t
arch_cpu_clock_epochoffset(void)
{
    return rtc_epochoffset;
}
