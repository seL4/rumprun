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


#include <sel4/helpers.h>
#include "sel4_stdio.h"

#include <utils/circular_buffer.h>



static circ_buf_t *buffer[RR_NUMIO];

int rumpcomp_sel4_stdio_init(int stdio) {
	if (stdio < 0 || stdio >= RR_NUMIO) {
		return -1;
	}

	/* Allocate buffer for storing input characters */
	buffer[stdio] = env.custom_simple.stdio_buf[stdio];
	return circ_buf_init(BIT(RR_STDIO_PAGE_BITS)-sizeof(*buffer[0]), buffer[stdio]);
}

int rumpcomp_sel4_stdio_puts(int stdio, char *buf, int len) {
		/* For each character call underlying putchar function */
	int n;
	for (n = 0; n < len; n++) {
		if (circ_buf_is_full(buffer[stdio])) {
			break;
		} else {
			circ_buf_put(buffer[stdio], *(buf+n));
		}
	}
    seL4_Signal(env.custom_simple.stdio_ep[stdio]);
	return n;
}

int rumpcomp_sel4_stdio_gets(int stdio, char *buf, int len) {
    int n;
	for (n = 0; n < len; n++) {
		if (circ_buf_is_empty(buffer[stdio])) {
			break;
		} else {
			*(buf+n) = circ_buf_get(buffer[stdio]);
		}
	}
	return n;
}


// TODO: Add remove handler function 
void rumpcomp_register_handler(void (*handler)(void))
{
	env.custom_simple.get_char_handler = handler;
}


