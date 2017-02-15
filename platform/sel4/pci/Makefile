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

RUMPTOP= ${TOPRUMP}

.include "${RUMPRUN_MKCONF}"
.include "${BUILDRUMP_TOOLFLAGS}"

RUMPCOMP_MAKEFILEINC_rumpdev_pci:= ${.PARSEDIR}/Makefile.pcihyperdefs
.export RUMPCOMP_MAKEFILEINC_rumpdev_pci

RUMPRUN_OBJDIR=${RROBJ}
RUMPRUN_TOOL_CFLAGS=${BUILDRUMP_TOOL_CFLAGS}

.export RUMPRUN_OBJDIR
.export RUMPRUN_TOOL_CFLAGS

.include "${RUMPTOP}/dev/Makefile.rumpdevcomp"

.for pcidev in ${RUMPPCIDEVS}
SUBDIR+= ${RUMPTOP}/dev/lib/lib${pcidev}
.endfor

.include <bsd.subdir.mk>
