#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

#  We use a second build directory for rump objects.
RUMP_BUILD_DIR ?= $(PROJECT_BASE)/build2
RUMPRUN_BASE_DIR ?= $(PROJECT_BASE)/rumprun/

# Installation directories for built rump kernel and rumprun toolchains,
# libraries and object files.
SEL4_RRDEST ?= $(RUMP_BUILD_DIR)/$(SEL4_ARCH)/rumprun
SEL4_RROBJ ?= $(RUMP_BUILD_DIR)/$(SEL4_ARCH)/sel4-obj

# `startfile` that is used when constructing rumprun images.  Contains all low
# level system symbols.  It is produced by the Makefile in ${RUMPRUN_BASE_DIR}/platform/sel4/
BASEFILE := $(SEL4_RROBJ)/rumprun.o

# Intermediate version of the basefile without CRT, FINI, and pre linking against any libraries
# This separation is so that CAmkES glue code can be linked with this file and then replacing
# the actual BASEFILE so that CAmkES glue code can be used in the bottom of our system.
# See at bottom of this file for example manual construction of Rumprun BASEFILE.
INTERMEDIATE_BASEFILE := $(SEL4_RROBJ)/rumprun-intermediate.o

# Required seL4 libraries for the bottom halve of the system
RUMPRUN_SEL4LIBS =  sel4 sel4muslcsys sel4vka sel4allocman \
       platsupport sel4platsupport platsupport sel4vspace \
	   sel4serialserver sel4test \
       sel4utils sel4simple-default sel4simple sel4debug utils cpio elf muslc

# Directory containing rumprun toolchains and other binaries.
RUMPRUN_BIN_DIR = $(SEL4_RRDEST)/bin
# Tooltuple of rumprun toolchains.  x86_64-rumprun-netbsd is an example
RUMPRUN_TOOLTUPLE := $(shell grep "TOOLTUPLE=" $(SEL4_RROBJ)/config.mk 2>/dev/null | cut  -c 11-)
# rumprun-bake binary name and path.
RUMPRUN_BAKE = $(RUMPRUN_BIN_DIR)/rumprun-bake
# rumprun toolchain compiler front end.
RUMPRUN_CC = $(RUMPRUN_BIN_DIR)/$(RUMPRUN_TOOLTUPLE)-gcc

# compiler front end for constructing the rumprun basefile manually.
# Note: this is a different compiler than RUMPRUN_CC mentioned above.
BASEFILE_CC := $(shell grep "CC=" $(SEL4_RROBJ)/config.mk 2>/dev/null | cut  -c 4-)

check-cc-option = $(shell if [ -z "`echo 'int p=1;' | $(CC) $(1) -S -o /dev/null -x c - 2>&1`" ]; \
                       then echo y; else echo n; fi)
# Disable PIE, but need to check if compiler supports it
LDFLAGS-$(call check-cc-option,-no-pie) += -no-pie

# Extra linker flags for constructing rumprun basefile manually.
BASEFILE_LIB_DIRS = $(SEL4_RROBJ)/lib/libbmk_core $(SEL4_RROBJ)/lib/libbmk_rumpuser
BASEFILE_LD_FLAGS = $(LDFLAGS-y) -Wl,-r \
 					$(BASEFILE_LIB_DIRS:%=-L%) \
					-u __vsyscall_ptr  \
					-Wl,--whole-archive \
					-lbmk_rumpuser -lbmk_core \
					-Wl,--no-whole-archive \
					-Wl,-dT ${RUMPRUN_BASE_DIR}/platform/sel4/stage1.lds

# Objcopy command used when constructing rumprun basefile manually.
BASEFILE_OBJCOPY = ${OBJCOPY} -w -G "bmk_*" -G "rumpuser_*" -G "jsmn_*" -G __assert_fail \
	     -G rumprun_platform_rumpuser_init -G _start -G env -G _zf_log_write_d -G _zf_log_write \
		 -G _zf_log_output_lvl -G "rumpns_*" -G "__aeabi*"

# Example manual construction of Rumprun BASEFILE:
# Create basefile binary
# ${CC} -nostdlib ${CFLAGS} ${CRTOBJFILES} ${INTERMEDIATE_BASEFILE} ${FINOBJFILES} -o ${BASEFILE} ${MAINOBJ_LD_FLAGS}
# Objcopy is used to promote all -G symbols to global symbols while hiding all other symbols.
# ${BASEFILE_OBJCOPY} $@
