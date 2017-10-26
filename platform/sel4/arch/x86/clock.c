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
#include <platsupport/arch/tsc.h>
#include <platsupport/timer.h>
#include <bmk-core/core.h>
#include <bmk-core/platform.h>
#include <bmk-core/printf.h>
#include <sel4/helpers.h>
#include <stdio.h>
/* RTC wall time offset at monotonic time base. */
static bmk_time_t rtc_epochoffset;
#define NSEC_PER_SEC	1000000000ULL

/*
 * Calculate prod = (a * b) where a is (64.0) fixed point and b is (0.32) fixed
 * point.  The intermediate product is (64.32) fixed point, discarding the
 * fractional bits leaves us with a (64.0) fixed point result.
 *
 * XXX Document what range of (a, b) is safe from overflow in this calculation.
 */
static uint64_t time_base;

static inline uint64_t
mul64_32(uint64_t a, uint32_t b)
{
    uint64_t prod;
#if defined(__x86_64__)
    /* For x86_64 the computation can be done using 64-bit multiply and
     * shift. */
    __asm__ (
        "mul %%rdx ; "
        "shrd $32, %%rdx, %%rax"
        : "=a" (prod)
        : "0" (a), "d" ((uint64_t)b)
    );
#elif defined(__i386__)
    /* For i386 we compute the partial products and add them up, discarding
     * the lower 32 bits of the product in the process. */
    uint32_t h = (uint32_t)(a >> 32);
    uint32_t l = (uint32_t)a;
    uint32_t t1, t2;
    __asm__ (
        "mul  %5       ; "  /* %edx:%eax = (l * b)                    */
        "mov  %4,%%eax ; "  /* %eax = h                               */
        "mov  %%edx,%4 ; "  /* t1 = ((l * b) >> 32)                   */
        "mul  %5       ; "  /* %edx:%eax = (h * b)                    */
        "xor  %5,%5    ; "  /* t2 = 0                                 */
        "add  %4,%%eax ; "  /* %eax = (h * b) + t1 (LSW)              */
        "adc  %5,%%edx ; "  /* %edx = (h * b) + t1 (MSW)              */
        : "=A" (prod), "=r" (t1), "=r" (t2)
        : "a" (l), "1" (h), "2" (b)
    );
#else
#error mul64_32 not supported for target architecture
#endif

    return prod;
}
uint64_t tsc_mult;
static uint64_t tsc_base;

void
x86_initclocks(void)
{

    uint64_t tsc_freq = simple_get_arch_info(&env.simple) * US_IN_S;
    if (!is_ltimer(&env.custom_simple)) {
        tsc_freq = env.custom_simple.timer_config.interface.tsc_freq;
    }

    tsc_base = rdtsc_pure();

    tsc_mult = (NSEC_PER_SEC << 32) / tsc_freq;
    time_base = mul64_32(tsc_base, tsc_mult);

    return;
}

/*
 * Return monotonic time since system boot in nanoseconds.
 */
bmk_time_t
bmk_platform_cpu_clock_monotonic(void)
{
    uint64_t tsc_now, tsc_delta;

    /*
     * Update time_base (monotonic time) and tsc_base (TSC time).
     */
    tsc_now = rdtsc_pure();

    tsc_delta = tsc_now - tsc_base;
    time_base += mul64_32(tsc_delta, tsc_mult);
    tsc_base = tsc_now;
    return time_base;
}

/*
 * Return epoch offset (wall time offset to monotonic clock start).
 */
bmk_time_t
bmk_platform_cpu_clock_epochoffset(void)
{

    return rtc_epochoffset;
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
     * Compute delta in PIT ticks. Return if it is less than minimum safe
     * amount of ticks.  Essentially this will cause us to spin until
     * the timeout.
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
