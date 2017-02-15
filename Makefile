#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
# @TAG(DATA61_BSD)
#

# We use a second build directory for rump objects.
ABS_TO_REL= python -c "import os.path; print os.path.relpath('$(1)', '$(2)')"
BUILD2_DIR = $(shell $(call ABS_TO_REL,$(PROJECT_BASE)/build2,$(SOURCE_DIR)))

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

#Build for release and 32 bit.
RUMPKERNEL_FLAGS+= -r -F ACLFLAGS=-m32

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


rumpsel4: $(STAGE_DIR)/lib/libmuslc.a $(COOKFS_REBUILD) $(RUMPFILES) $(PROJECT_BASE)/.config
	@echo "[Installing] headers"
	cp -r $(SEL4_INSTALL_HEADERS) $(STAGE_DIR)/include/.
	@echo "[Building rumprun]"
	cd $(SOURCE_DIR) && env -i PATH=${PATH2} PROJECT_BASE=$(PROJECT_BASE) CC=gcc ./build-rr.sh \
	-d $(BUILD2_DIR)/rumprun2 -o $(BUILD2_DIR)/sel4-obj sel4 -- $(RUMPKERNEL_FLAGS)
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
