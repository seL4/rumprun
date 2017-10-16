/*-
 * Copyright (c) 2013, 2014 Antti Kantee.  All Rights Reserved.
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

#include <hw/types.h>
#include <hw/kernel.h>

#include <bmk-core/pgalloc.h>

#include <bmk-pcpu/pcpu.h>

#include <bmk-rumpuser/core_types.h>

#include "pci_user.h"

#define PCI_CONF_ADDR 0xcf8
#define PCI_CONF_DATA 0xcfc

static struct {
    int intrs;
    int bus;
    int dev;
    int function;
} pci_data[BMK_MAXINTR];

int
rumpcomp_pci_port_out(uint32_t port, int io_size, uint32_t val) {
	switch (io_size) {
		case 1:
			__asm__ __volatile__("outb %0, %1" :: "a"((uint8_t)val), "d"((uint16_t)port));
		break;
		case 2:
			__asm__ __volatile__("out %0, %1" :: "a"((uint16_t)val), "d"((uint16_t)port));

		break;
		case 4:
			__asm__ __volatile__("outl %0, %1" :: "a"((uint32_t)val), "d"((uint16_t)port));
		break;
	}
	return 0;
}

int
rumpcomp_pci_intr_type(void)
{
	return 0;  //PCI_INTR_TYPE_INTX;
}

int
rumpcomp_pci_get_bdf(unsigned cookie, unsigned *bus, unsigned *dev, unsigned *function) {
    if (cookie > BMK_MAXINTR) {
        return 1;
    }

    *bus = pci_data[cookie].bus;
    *dev = pci_data[cookie].dev;
    *function = pci_data[cookie].function;
    return 0;
}

int
rumpcomp_pci_port_in(uint32_t port, int io_size, uint32_t *result) {
	uint32_t res = 0;
	switch (io_size){
		case 1:
			__asm__ __volatile__("inb %%dx, %%al" : "=a"(res) : "d"((uint16_t)port));
		break;
		case 2:
			__asm__ __volatile__("in %1, %0" : "=a"(res) : "d"((uint16_t)port));

		break;
		case 4:
			__asm__ __volatile__("inl %1, %0" : "=a"(res) : "d"((uint16_t)port));
		break;
	}
	*result = res;
	return 0;
}


int
rumpcomp_pci_iospace_init(void)
{

	return 0;
}

static uint32_t
makeaddr(unsigned bus, unsigned dev, unsigned fun, int reg)
{

	return (1<<31) | (bus<<16) | (dev <<11) | (fun<<8) | (reg & 0xfc);
}

int
rumpcomp_pci_confread(unsigned bus, unsigned dev, unsigned fun, int reg,
	unsigned int *value)
{
	uint32_t addr;
	unsigned int data;

	addr = makeaddr(bus, dev, fun, reg);
	outl(PCI_CONF_ADDR, addr);
	data = inl(PCI_CONF_DATA);

	*value = data;
	return 0;
}

int
rumpcomp_pci_confwrite(unsigned bus, unsigned dev, unsigned fun, int reg,
	unsigned int value)
{
	uint32_t addr;

	addr = makeaddr(bus, dev, fun, reg);
	outl(PCI_CONF_ADDR, addr);
	outl(PCI_CONF_DATA, value);

	return 0;
}

static int intrs[BMK_MAXINTR];

int
rumpcomp_pci_irq_map(unsigned bus, unsigned device, unsigned fun,
	int intrline, unsigned cookie)
{

	if (cookie > BMK_MAXINTR)
		return BMK_EGENERIC;

	intrs[cookie] = intrline;

    pci_data[cookie].intrs = intrline;
    pci_data[cookie].bus = bus;
    pci_data[cookie].dev = device;
    pci_data[cookie].function = fun;

	return 0;
}

void *
rumpcomp_pci_irq_establish(unsigned cookie, int (*handler)(void *), void *data)
{

	bmk_isr_rumpkernel(handler, data, intrs[cookie], BMK_INTR_ROUTED);
	return &intrs[cookie];
}

/*
 * Well at least there's some benefit to running on physical memory.
 * This stuff is really trivial.
 */

void *
rumpcomp_pci_map(unsigned long addr, unsigned long len)
{

	return (void *)addr;
}

void
rumpcomp_pci_unmap(void *addr)
{

}
