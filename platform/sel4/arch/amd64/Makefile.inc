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


SRCS+=	arch/x86/clock.c arch/x86/arch.c

CFLAGS+=

${RROBJ}/platform/archdirs.stamp:
	$(Q)mkdir -p ${RROBJ}/platform/arch/amd64
	$(Q)mkdir -p ${RROBJ}/platform/arch/x86
	$(Q)touch $@
