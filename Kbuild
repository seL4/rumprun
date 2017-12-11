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
ABS_TO_REL= python -c "import os.path; print os.path.relpath('$(1)', '$(2)')"

include ${CURRENT_DIR}/platform/sel4/rumprunlibs.mk

ifeq ($(CONFIG_RUMPRUN), y)

mrproper: clean_rump

.PHONY:clean_rump
clean_rump:
	@echo " [Rumprun] Deleting rumprun build objects"
	$Qrm -rf $(SEL4_RROBJ)
	$Qrm -rf $(SEL4_RRDEST)
	$Qrm -rf ${CURRENT_DIR}/.rumpstamp

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


${CURRENT_DIR}/.rumpstamp:
	@echo "[rumprun: Updating rumprun sources]"
	(cd ${CURRENT_DIR} && ./init-sources.sh)
	@echo "[rumprun: Update complete]"

# All rumprun source directories.
SRC_DIRECTORIES := app-tools buildrump.sh include lib platform src-netbsd
# Find all source rump files.
RUMPFILES += $(shell find -L $(SRC_DIRECTORIES:%=$(CURRENT_DIR)/%) \( -type f \))
RUMPFILES += $(shell find $(CURRENT_DIR) -maxdepth 1 -type f)

# Force rebuild if RUMPSTALE is set (For when the COOKFS directory is updated)
ifeq ($(RUMPSTALE), 1)
COOKFS_REBUILD := stale
endif

ifeq ($(SEL4_ARCH), ia32)
RUMPKERNEL_FLAGS+= -F ACLFLAGS=-m32
endif

ifeq ($(CONFIG_USER_DEBUG_BUILD),)
#Build for release.
RUMPKERNEL_FLAGS+= -r
endif

#Change TLS model to avoid unnecessary seL4 invocations.
RUMPKERNEL_FLAGS+= -F CFLAGS=-mno-tls-direct-seg-refs -F CFLAGS=-ftls-model=global-dynamic

SEL4_INSTALL_HEADERS := $(CURRENT_DIR)/platform/sel4/include/sel4/rumprun

# Suppress rump build output unless V is set
ifeq ($(V),)
QUIET:=-q -q
endif

# This wraps the call to ccache in a shell script because the rumprun build doesn't
# expand the variable correctly if we try CC=ccache gcc.
# See here for more info: https://wiki.netbsd.org/tutorials/using_ccache_with_build_sh/
ccache_wrapper_contents = \
\#!/bin/sh \n\
exec $(CCACHE) $1 \"\$$@\"\n

$(BUILD_BASE)/rumprun/%-wrapper:
	mkdir -p $(BUILD_BASE)/rumprun
	echo -e "$(call ccache_wrapper_contents, $*)" | sed -e 's/^[ ]//' >$(@)
	chmod +x $@

LDFLAGS_SEL4:= -L$(STAGE_BASE)/lib $(RUMPRUN_SEL4LIBS:%=-l%)
CRTOBJFILES_SEL4 := $(STAGE_BASE)/lib/crt1.o $(STAGE_BASE)/lib/crti.o $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtbegin.o)
FINOBJFILES_SEL4 := $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtend.o) $(STAGE_BASE)/lib/crtn.o
CFLAGS_SEL4:=-I$(PROJECT_BASE)/stage/x86/pc99/include

rumprun: $(libc) libsel4 libcpio libelf libsel4muslcsys libsel4vka libsel4allocman \
       libplatsupport libsel4platsupport libsel4vspace \
       libsel4utils libsel4simple libutils libsel4debug libsel4sync libsel4serialserver libsel4test \
       ${CURRENT_DIR}/.rumpstamp \
       $(STAGE_BASE)/lib/libmuslc.a $(COOKFS_REBUILD) $(RUMPFILES) $(PROJECT_BASE)/.config \
	$(BUILD_BASE)/rumprun/$(CROSS_COMPILE)gcc-wrapper $(BUILD_BASE)/rumprun/$(CROSS_COMPILE)g++-wrapper
	@echo "[Installing] headers"
	cp -r $(SEL4_INSTALL_HEADERS) $(STAGE_BASE)/include/.
	@echo "[Building rumprun]"
	cd $(CURRENT_DIR) && env -i \
	PATH=${PATH} \
	SEL4_ARCH=$(SEL4_ARCH) \
	PROJECT_BASE=$(PWD) \
	CC=$(BUILD_BASE)/$@/$(CROSS_COMPILE)gcc-wrapper \
	CXX=$(BUILD_BASE)/$@/$(CROSS_COMPILE)g++-wrapper \
	CFLAGS_SEL4=$(CFLAGS_SEL4) LDFLAGS_SEL4="$(LDFLAGS_SEL4)" \
	CRTOBJFILES_SEL4="$(CRTOBJFILES_SEL4)" FINOBJFILES_SEL4="$(FINOBJFILES_SEL4)" \
	./build-rr.sh $(QUIET) \
	-d $(shell $(call ABS_TO_REL,$(SEL4_RRDEST),$(CURRENT_DIR))) \
	-o $(shell $(call ABS_TO_REL,$(SEL4_RROBJ),$(CURRENT_DIR))) \
	sel4 -- $(RUMPKERNEL_FLAGS)
	@echo " [rumprun] rebuilt rumprun sel4"




.PHONY: stale
stale:
	@echo " [rumprun] cookfs directory was updated"

# Rename muslc's archive from libc.a to libmuslc.a (Don't ask)
$(STAGE_BASE)/lib/libmuslc.a:
	cp $(STAGE_BASE)/lib/libc.a $(STAGE_BASE)/lib/libmuslc.a
