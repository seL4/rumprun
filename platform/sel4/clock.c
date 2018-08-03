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

/*-
 * Copyright (c) 2014, 2015 Antti Kantee.  All Rights Reserved.
 * Copyright (c) 2015 Martin Lucina.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sel4/kernel.h>

#include <utils/util.h>
#include <platsupport/timer.h>
#include <bmk-core/core.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <sel4/helpers.h>
#include <stdio.h>

/*
 * Return monotonic time since system boot in nanoseconds.
 */
bmk_time_t
bmk_platform_cpu_clock_monotonic(void)
{
    return arch_cpu_clock_monotonic();
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
bmk_time_t
bmk_platform_cpu_clock_epochoffset(void)
{
    return arch_cpu_clock_epochoffset();
}

/*
 * Block the CPU until monotonic time is *no later than* the specified time.
 * Returns early if any interrupts are serviced, or if the requested delay is
 * too short.
 */
//This is only called in schedule if there is no runnable thread.
void
bmk_platform_cpu_block(bmk_time_t until)
{
    bmk_time_t now, delta_ns;
    bmk_assert(env.spldepth > 0);

    /*
     * Return if called too late.  Doing do ensures that the time
     * delta is positive.
     */
    now = bmk_platform_cpu_clock_monotonic();
    if (until <= now) {
        return;
    }

    /*
     * Compute the delta between time to wake and now. Setup a timeout via RPC to the
     * timer server.
     */
    delta_ns = until - now;
    int res;
    if (is_ltimer(&env.custom_simple)) {
        res = ltimer_set_timeout(&env.custom_simple.timer_config.ltimer.ltimer, delta_ns, TIMEOUT_RELATIVE);
    } else {
        res = env.custom_simple.timer_config.interface.oneshot_relative(0, delta_ns);
    }
    if (res != 0) {
        bmk_platform_splx(0);
        bmk_platform_splhigh();
        return;
    }

    env.should_wakeup = 1;
    bmk_platform_splx(0);
    seL4_Wait(env.custom_simple.timer_config.timer_ntfn, NULL);

    env.should_wakeup = 0;

    bmk_platform_splhigh();
}
