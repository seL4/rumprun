#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

.include <bsd.own.mk>

LIB=	rumpsel4_stdio

SRCS+=	sel4_stdio.c
.include "${RUMPRUN_MKCONF}"
RUMPRUN_OBJDIR=${RROBJ}

RUMPRUN_CURRENT_DIR:= ${.PARSEDIR}

RUMPTOP= ${TOPRUMP}
RUMPCOMP_USER_SRCS=	sel4_stdio_user.c
RUMPCOMP_USER_CPPFLAGS+=${CFLAGS_SEL4}
RUMPCOMP_USER_CPPFLAGS+=-I${RUMPRUN_CURRENT_DIR}/../include
RUMPCOMP_USER_CPPFLAGS+=-I${RUMPRUN_CURRENT_DIR}/../../../include
RUMPCOMP_USER_CPPFLAGS+=-I${RUMPRUN_OBJDIR}/include

.undef RUMPKERN_ONLY

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>