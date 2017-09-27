#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
# @TAG(DATA61_BSD)
#
CURRENT_DIR := $(dir $(abspath $(lastword ${MAKEFILE_LIST})))
PROJECT_BASE := $(PWD)

include ${CURRENT_DIR}/platform/sel4/rumprunlibs.mk

ifeq ($(CONFIG_RUMPRUN), y)
# These following rules are for loading a directory into a rumprun image
# via the librumprunfs library.  Setting CONFIG_RUMPRUN_COOKFS_DIR to a
# valid path from the root of the project will result in that folder appearing
# at the top of the rumprun root file system /(folder name)

cookfs_dirpath := $(CONFIG_RUMPRUN_COOKFS_DIR)
ifeq ($(cookfs_dirpath),)
cookfs_dirpath = ""
endif
ifneq ($(cookfs_dirpath),"")
FULLDIRPATH := $(PWD)/$(cookfs_dirpath)
else
endif
cookfs-dir := $(shell mkdir -p $(SEL4_RROBJ)/rootfs/ && rsync -av $(FULLDIRPATH) \
	$(CURRENT_DIR)/lib/librumprunfs_base/rootfs/ $(SEL4_RROBJ)/rootfs/ --delete | wc -l)
ifneq ($(cookfs-dir), 4)
	export RUMPSTALE = 1
endif
endif


libs-$(CONFIG_RUMPRUN) += rumprun

rumprun: $(libc) libsel4 libcpio libelf libsel4muslcsys libsel4vka libsel4allocman \
       libplatsupport libsel4platsupport libsel4vspace \
       libsel4utils libsel4simple libutils libsel4debug libsel4sync libsel4serialserver libsel4test
