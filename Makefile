#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
# @TAG(DATA61_BSD)
#

PROJECT_BASE := $(PWD)
include $(SOURCE_DIR)/platform/sel4/rumprunlibs.mk
ABS_TO_REL= python -c "import os.path; print os.path.relpath('$(1)', '$(2)')"

# Save the path
PATH2 := ${PATH}
# All rumprun source directories.
SRC_DIRECTORIES := app-tools buildrump.sh include lib platform src-netbsd ../libs
# Find all source rump files.
RUMPFILES += $(shell find -L $(SRC_DIRECTORIES:%=$(SOURCE_DIR)/%) \( -type f \))
RUMPFILES += $(shell find $(SOURCE_DIR) -maxdepth 1 -type f)

# Force rebuild if RUMPSTALE is set (For when the COOKFS directory is updated)
ifeq ($(RUMPSTALE), 1)
COOKFS_REBUILD := stale
endif

supported= false

ifeq ($(SEL4_ARCH), ia32)
supported:=true
RUMPKERNEL_FLAGS+= -F ACLFLAGS=-m32
endif
ifeq ($(SEL4_ARCH), x86_64)
supported:=true
endif
ifneq (${supported},true)
$(error only supported target is x86, ${SEL4_ARCH} is not supported)
endif

#Build for release and 32 bit.
RUMPKERNEL_FLAGS+= -r

ifeq ($(RTARGET), sel4)
#Change TLS model to avoid unnecessary seL4 invocations.
RUMPKERNEL_FLAGS+= -F CFLAGS=-mno-tls-direct-seg-refs -F CFLAGS=-ftls-model=global-dynamic
all: rumpsel4
	echo " [rumprun] done"
else
all:rumphw
	echo " [rumprun] done"
endif

SEL4_INSTALL_HEADERS := $(SOURCE_DIR)/platform/sel4/include/sel4/rumprun

# Rule for update sources on first build
# Note: This creates a stamp file in the top level source directory
$(SOURCE_DIR)/.rumpstamp:
	cd $(SOURCE_DIR) && git submodule init
	cd $(SOURCE_DIR) && git submodule update
	cd $(SOURCE_DIR)/src-netbsd && git am ../src-netbsd.patches/*
	touch $@

rumpsel4: $(STAGE_DIR)/lib/libmuslc.a $(COOKFS_REBUILD) $(RUMPFILES) $(PROJECT_BASE)/.config $(SOURCE_DIR)/.rumpstamp
	@echo "[Installing] headers"
	cp -r $(SEL4_INSTALL_HEADERS) $(STAGE_DIR)/include/.
	@echo "[Building rumprun]"
	cd $(SOURCE_DIR) && env -i PATH=${PATH2} SEL4_ARCH=$(SEL4_ARCH) PROJECT_BASE=$(PWD) CC=gcc ./build-rr.sh \
	-d $(shell $(call ABS_TO_REL,$(SEL4_RRDEST),$(SOURCE_DIR))) \
	-o $(shell $(call ABS_TO_REL,$(SEL4_RROBJ),$(SOURCE_DIR))) \
	sel4 -- $(RUMPKERNEL_FLAGS)
	@echo " [rumprun] rebuilt rumprun sel4"
	touch rumpsel4


rumphw: $(COOKFS_REBUILD) $(RUMPFILES) $(PROJECT_BASE)/.config
	@echo "[Building rumprun]"
	cd $(SOURCE_DIR) && env -i PATH=${PATH2} PROJECT_BASE=$(PROJECT_BASE) \
	CC=gcc ./build-rr.sh -d $(BUILD2_DIR)/rumprun -o $(BUILD2_DIR)/hw-obj hw -- $(RUMPKERNEL_FLAGS)
	@echo " [rumprun] rebuilt rumprun hw"
	touch rumphw


.PHONY: stale
stale:
	@echo " [rumprun] cookfs directory was updated"

# Rename muslc's archive from libc.a to libmuslc.a (Don't ask)
$(STAGE_DIR)/lib/libmuslc.a:
	cp $(STAGE_DIR)/lib/libc.a $(STAGE_DIR)/lib/libmuslc.a
