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


#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <rump-sys/vfs.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <sys/stat.h>
#include <rump-sys/kern.h>
#include "sel4_stdio.h"

/*
	This file implements stdio device files that are accessed through the 
	files /dev/stdin, /dev/stdout, /dev/stderr.  Its purpose is to provide input and output over an underlying
	circular buffer using seL4 notification objects for signalling. 
	 Currently read and write are supported.  Read will block if 
	no input data is available.  There is currently no way to check if data is available
	to be read without blocking. 
*/

MODULE(MODULE_CLASS_DRIVER, sel4_stdio, NULL);

static struct rumpuser_cv *cvp_serial;
static struct rumpuser_mtx *mtxp_serial;


/* Interrupt handler, it signals a thread waiting for data available in sel4_stdio_read */
static void get_char_handler(void) {
    rumpuser_cv_signal(cvp_serial);
}

static int
sel4_stdio_open(dev_t dev, int flag, int mode, struct lwp *l)
{
	static bool open_once[RR_NUMIO] = {0};

	if (minor(dev) < 0 || minor(dev) >= RR_NUMIO) {
		panic("sel4_stdio: invalid minor device number");
	}
	if (open_once[minor(dev)]) {
		panic("sel4_stdio only supports each file being opened once and never closed");
	}
	open_once[minor(dev)] = true;

	if (rumpcomp_sel4_stdio_init(minor(dev))) {
		aprint_error("Failed to init sel4_stdio");
		return -1;
	}

	/* If stdin */
	if (minor(dev) == RR_STDIN) {
		/* Create condition variable and mutex for interrupt handling syncronisation */
		rumpuser_cv_init(&cvp_serial);
		rumpuser_mutex_init(&mtxp_serial, RUMPUSER_MTX_SPIN);
		rumpuser_mutex_enter(mtxp_serial);
		/* Register input interrupt handler */
		rumpcomp_register_handler(get_char_handler);
	}
	return 0;
}

/* TODO Deal with carrige return stuff */
static int
sel4_stdio_read(dev_t dev, struct uio *uio, int flag)
{
	char *buf;
	size_t len, n;
	int error = 0;
	if (minor(dev) != RR_STDIN) {
		aprint_error("Only stdin supports read");
		return -1;
	}

	/* Allocate buffer for read */
	buf = kmem_alloc(PAGE_SIZE, KM_SLEEP);
	if (buf == NULL) {
		aprint_error("kmem_alloc returned null");
		return -1;
	}
	n = 0;	
	len = min(PAGE_SIZE, uio->uio_resid);
	/* Try and dequeue characters from the buffer
	   If no characters are available, block on a condition variable
	   When characters become available, wake up and read out characters.
	   Blocking only occurs if no characters have been read */
	while (n < len ) {
		n += rumpcomp_sel4_stdio_gets(minor(dev), buf, len);
		if (n == 0) {
			rumpuser_cv_wait(cvp_serial, mtxp_serial);
		} else {
			break;
		}
	}
	/* Move read characters into user buffers */
	error = uiomove(buf, n, uio);
	
	kmem_free(buf, PAGE_SIZE);

	return error;
}

static int
sel4_stdio_write(dev_t dev, struct uio *uio, int flag)
{

	char *buf;
	size_t len;
	int error = 0;
	if (minor(dev) == RR_STDIN) {
		aprint_error("stdin doesn't support write");
		return -1;
	}
	/* Allocate write buffer */
	buf = kmem_alloc(PAGE_SIZE, KM_SLEEP);
	if (buf == NULL) {
		aprint_error("kmem_alloc returned null");
		return -1;
	}

	while (uio->uio_resid > 0) {
		len = min(PAGE_SIZE, uio->uio_resid);
		error = uiomove(buf, len, uio);
		if (error)
			break;
		rumpcomp_sel4_stdio_puts(minor(dev), buf, len);
	}
	kmem_free(buf, PAGE_SIZE);

	return error;

}

static int
sel4_stdio_ioctl(dev_t dev, u_long cmd, void *addr, int flag, struct lwp *l)
{
	/* We do not support tty */
	if (cmd == TIOCGETA)
		return 0;

	return ENOTTY;

}

static int
sel4_stdio_poll(dev_t dev, int events, struct lwp *l)
{
	/* No poll currently supported */
	aprint_normal("poll called.\n");

	return -1;
}

static int
sel4_stdio_kqfilter(dev_t dev, struct knote *kn)
{
	/* no kqfilter currently supported */
	aprint_normal("ioctl kqfilter.\n");

	return -1;
}


const struct cdevsw sel4_stdio = {
	.d_open = sel4_stdio_open,
	.d_close = nullclose,
	.d_read = sel4_stdio_read,
	.d_write = sel4_stdio_write,
	.d_ioctl = sel4_stdio_ioctl,
	.d_stop = nostop,
	.d_tty = notty,
	.d_poll = sel4_stdio_poll,
	.d_mmap = nommap,
	.d_kqfilter = sel4_stdio_kqfilter,
	.d_discard = nodiscard,
	.d_flag = D_TTY
};



static int
sel4_stdio_modcmd(modcmd_t cmd, void *arg)
{
	devmajor_t bmaj, cmaj;

	bmaj = cmaj = -1;
	/* Register cdevsw with cmaj driver number. */
	FLAWLESSCALL(devsw_attach("sel4_stdio", NULL, &bmaj, &sel4_stdio, &cmaj));
	/* Create character device node using cmaj number we were assigned */
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/stdin", cmaj, RR_STDIN));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/stdout", cmaj, RR_STDOUT));
	FLAWLESSCALL(rump_vfs_makeonedevnode(S_IFCHR, "/dev/stderr", cmaj, RR_STDERR));
	/* Print to init output */
	aprint_normal("sel4_stdio: Created stdio device files.\n");
	return 0;
}