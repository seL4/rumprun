#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
# @TAG(DATA61_BSD)
#
#
include ../../global.mk
include ${BUILDRUMP_TOOLFLAGS}

default: all

# Check if we're building for a supported target.
supported= false
MACHINE?= i386
ifeq (${MACHINE},i386)
SEL4_MACHINE_ARCH=i386
supported:= true
endif
ifeq (${MACHINE},amd64)
supported:= true
endif
ifneq (${supported},true)
$(error only supported target is x86, you have ${MACHINE})
endif

ARCHDIR?= ${MACHINE}
SEL4_MACHINE_ARCH?= ${MACHINE_GNU_ARCH}


LDSCRIPT:=	$(abspath arch/${ARCHDIR}/kern.ldscript)
SRCS+=		entry.c undefs.c kernel.c intr.c
include ../Makefile.inc
include arch/${ARCHDIR}/Makefile.inc

# Add seL4 header files
CFLAGS+= -I$(PROJECT_BASE)/stage/x86/pc99/include
OBJS:=	$(patsubst %.c,${RROBJ}/platform/%.o,${SRCS}) \
	$(patsubst %.S,${RROBJ}/platform/%.o,${ASMS})

SEL4_INSTALL_HEADERS := include/sel4/rumprun

.PHONY:	clean cleandir all

all: links archdirs ${MAINOBJ} ${TARGETS}

# These are required to provide hardware info to rumprun/libs/*
${RROBJ}/include/sel4/machine:
	@mkdir -p ${RROBJ}/include/sel4
	@ln -sf $(shell pwd)/include/arch/${SEL4_MACHINE_ARCH} $@

${RROBJ}/include/bmk-pcpu:
	@ln -sf ${RROBJ}/include/sel4/machine $@

links: ${RROBJ}/include/sel4/machine ${RROBJ}/include/bmk-pcpu

${RROBJ}/platform/%.o: %.c
	$(Q)$(call make-depend,$<,$@,$(patsubst %.o,%.d,$@))
	${CC} ${CPPFLAGS} ${CFLAGS} -c $< -o $@

${RROBJ}/platform/%.o: %.S
	$(Q)$(call make-depend,$<,$@,$(patsubst %.o,%.d,$@))
	${CC} -D_LOCORE ${CPPFLAGS} ${CFLAGS} -c $< -o $@

SEL4LIBS = muslc sel4 cpio elf sel4muslcsys sel4vka sel4allocman \
       platsupport sel4platsupport sel4vspace \
       sel4utils sel4simple utils sel4debug sel4simple-default

SEL4_LIBDIR = $(PROJECT_BASE)/stage/x86/pc99/lib
CRTOBJFILES ?= $(SEL4_LIBDIR)/crt1.o $(SEL4_LIBDIR)/crti.o
FINOBJFILES ?= $(SEL4_LIBDIR)/crtn.o

# Look away now
${MAINOBJ}: ${OBJS} platformlibs
	echo "Building: $(MAINOBJ): $(CFLAGS)"
	# Generate the rumprun.o object file that is linked against apps that want to use rumprun.
	# --whole-archive is used to ensure constructor symbols are included from some libraries
	# --start-group because seL4 libraries have circular dependencies
	${CC} -nostdlib ${CFLAGS} -Wl,-r ${CRTOBJFILES} ${OBJS} ${FINOBJFILES} -o $@ \
	    -L${RROBJLIB}/libbmk_core -L${RROBJLIB}/libbmk_rumpuser -L$(SEL4_LIBDIR) \
	    -Wl,--undefined=__vsyscall_ptr -Wl,--start-group $(SEL4LIBS:%=-l%) \
		-Wl,--end-group -Wl,--whole-archive -lbmk_rumpuser -lbmk_core -Wl,--no-whole-archive
	# Objcopy is used to promote all -G symbols to global symbols while hiding all other symbols.
	${OBJCOPY} -w -G bmk_* -G rumpuser_* -G jsmn_* -G __assert_fail \
	     -G rumprun_platform_rumpuser_init -G _start -G env -G _zf_log_write_d -G _zf_log_write -G _zf_log_output_lvl $@
	# We need to rename all constructors that muslc and seL4 libraries use so that the symbols and
	# sections don't have name collisions with netbsd libc and apps that get linked later.
	# This objdump dumps and extracts the constructor sections and then objcopy renames them.
	${OBJDUMP} -h $@ |\
	grep \.init_array |\
	 awk '{print $$2}'|\
	 cut -d'.' -f 3 |\
	 xargs -I {} ${OBJCOPY} --rename-section .init_array.{}=.init_array_sel4.{} $@
	# Renames the __init_array start and end symbols that muslc calls so that we can have two C libraries
	# initialised in the same address space.
	${OBJCOPY} --redefine-sym __init_array_start=__init_array_start_sel4 --redefine-sym __init_array_end=__init_array_end_sel4 $@

clean: commonclean
	rm -f ${OBJS_BMK} include/sel4/machine buildtest ${MAINOBJ}

cleandir: clean


DEPS = $(patsubst %.c,${RROBJ}/platform/%.d,$(SRCS)) $(patsubst %.cxx,${RROBJ}/platform/%.d,$(CXXFILES)) $(patsubst %.S,${RROBJ}/platform/%.d,$(ASMFILES))

ifneq "$(MAKECMDGOALS)" "clean"
  -include ${DEPS}
endif

# $(call make-depend,source-file,object-file,depend-file)
define make-depend
  ${CC} -MM            \
         -MF $3         \
         -MP            \
         -MT $2         \
         $(CFLAGS)      \
         $(CPPFLAGS)    \
         $1
endef
define make-cxx-depend
  ${CXX} -MM            \
         -MF $3         \
         -MP            \
         -MT $2         \
         $(CXXFLAGS)    \
         $(CPPFLAGS)    \
         $1
endef
