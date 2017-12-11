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
RUMPRUN_BUILD_DIR := $(BUILD_BASE)/rumprun

mrproper: clean_rump

.PHONY:clean_rump
clean_rump:
	@echo " [Rumprun] Deleting rumprun build objects"
	$Qrm -rf $(SEL4_RROBJ)
	$Qrm -rf $(SEL4_RRDEST)
	$Qrm -rf ${CURRENT_DIR}/.rumpstamp

${CURRENT_DIR}/.rumpstamp:
	@echo "[rumprun: Updating rumprun sources]"
	(cd ${CURRENT_DIR} && ./init-sources.sh)
	@echo "[rumprun: Update complete]"

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

$(RUMPRUN_BUILD_DIR)/%-wrapper:
	mkdir -p $(RUMPRUN_BUILD_DIR)
	echo -e "$(call ccache_wrapper_contents, $*)" | sed -e 's/^[ ]//' >$(@)
	chmod +x $@

LDFLAGS_SEL4:= -L$(STAGE_BASE)/lib $(RUMPRUN_SEL4LIBS:%=-l%)
CRTOBJFILES_SEL4 := $(STAGE_BASE)/lib/crt1.o $(STAGE_BASE)/lib/crti.o $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtbegin.o)
FINOBJFILES_SEL4 := $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtend.o) $(STAGE_BASE)/lib/crtn.o
CFLAGS_SEL4:=-I$(PROJECT_BASE)/stage/x86/pc99/include
RR_ENV_VARS := PATH=${PATH} SEL4_ARCH=$(SEL4_ARCH) PROJECT_BASE=$(PWD) CC=$(RUMPRUN_BUILD_DIR)/$(CROSS_COMPILE)gcc-wrapper \
	CXX=$(RUMPRUN_BUILD_DIR)/$(CROSS_COMPILE)g++-wrapper CFLAGS_SEL4=$(CFLAGS_SEL4) \
	LDFLAGS_SEL4="$(LDFLAGS_SEL4)" 	CRTOBJFILES_SEL4="$(CRTOBJFILES_SEL4)" 	FINOBJFILES_SEL4="$(FINOBJFILES_SEL4)"


BUILD_RR_CMD_LINE = cd $(CURRENT_DIR) && env -i $(RR_ENV_VARS) ./build-rr.sh $(QUIET) \
	-d $(shell $(call ABS_TO_REL,$(SEL4_RRDEST),$(CURRENT_DIR))) \
	-o $(shell $(call ABS_TO_REL,$(SEL4_RROBJ),$(CURRENT_DIR))) \
	sel4 -- $(RUMPKERNEL_FLAGS)

BUILD_RR_FILES:= $(shell find -L $(CURRENT_DIR)/buildrump.sh/ \( -type f \))
SRC_NETBSD_FILES:= $(shell find -L $(CURRENT_DIR)/src-netbsd/ \( -type f \))
SEL4_PLATFORM_FILES:= $(shell find -L $(CURRENT_DIR)/platform/sel4/ \( -type f \)) $(CURRENT_DIR)/platform/Makefile.inc
RUMPRUN_LIB_FILES:= $(shell find -L $(CURRENT_DIR)/lib/ \( -type f \))
RUMPRUN_INCLUDE_FILES:= $(shell find -L $(CURRENT_DIR)/include/ \( -type f \))
APP_TOOLS_FILES:= $(shell find -L $(CURRENT_DIR)/app-tools/ \( -type f \))
RUMPRUN_FILES:= $(SEL4_PLATFORM_FILES) $(RUMPRUN_LIB_FILES) $(RUMPRUN_INCLUDE_FILES)

$(RUMPRUN_BUILD_DIR)/install_headers: $(shell find -L $(SEL4_INSTALL_HEADERS) \( -type f \))
	@echo "[Installing] headers"
	@mkdir -p $(STAGE_BASE)/include/
	$(Q)cp -ra $(SEL4_INSTALL_HEADERS) $(STAGE_BASE)/include/.
	$(Q)mkdir -p $(RUMPRUN_BUILD_DIR)
	$(Q)touch $@


# Only set FULLDIRPATH if the COOKFS dir is set to something proper
ifneq ($(CONFIG_RUMPRUN_COOKFS_DIR),"")
ifneq ($(CONFIG_RUMPRUN_COOKFS_DIR),)
FULLDIRPATH := $(PWD)/$(CONFIG_RUMPRUN_COOKFS_DIR)
endif
endif

.PHONY: rumprun-setup-librumprunfs
rumprun-setup-librumprunfs:
	$(Q)mkdir -p $(SEL4_RROBJ)/rootfs/ && rsync -av $(FULLDIRPATH) \
	$(CURRENT_DIR)/lib/librumprunfs_base/rootfs/ $(SEL4_RROBJ)/rootfs/ --delete


rumprun: $(libc) libsel4 libcpio libelf libsel4muslcsys libsel4vka libsel4allocman \
       libplatsupport libsel4platsupport libsel4vspace \
       libsel4utils libsel4simple libutils libsel4debug libsel4sync libsel4serialserver libsel4test \
       ${CURRENT_DIR}/.rumpstamp \
       $(STAGE_BASE)/lib/libmuslc.a rumprun-setup-librumprunfs $(PROJECT_BASE)/.config \
	   $(RUMPRUN_BUILD_DIR)/$(CROSS_COMPILE)gcc-wrapper $(RUMPRUN_BUILD_DIR)/$(CROSS_COMPILE)g++-wrapper \
	   $(BUILD_RR_FILES) $(SRC_NETBSD_FILES) $(APP_TOOLS_FILES) $(RUMPRUN_FILES) \
	   $(RUMPRUN_BUILD_DIR)/install_headers
	@echo "[Building rumprun]"
	${BUILD_RR_CMD_LINE}
	@echo " [rumprun] rebuilt rumprun sel4"


# Rename muslc's archive from libc.a to libmuslc.a (Don't ask)
$(STAGE_BASE)/lib/libmuslc.a:
	cp $(STAGE_BASE)/lib/libc.a $(STAGE_BASE)/lib/libmuslc.a
