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


LDSCRIPT:=	$(abspath stage2.lds)
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


include rumprunlibs.mk

ifneq ($(abspath $(MAINOBJ)),$(abspath $(BASEFILE)))
$(error $(BASEFILE) does not equal $(MAINOBJ). ${PWD})
endif


${INTERMEDIATE_BASEFILE}: ${OBJS} platformlibs
	echo "Building: $(INTERMEDIATE_BASEFILE)"
	${CC} -nostdlib ${CFLAGS} ${OBJS} -o $@ -Wl,-r

SEL4_LIBDIR = $(PROJECT_BASE)/stage/x86/pc99/lib
CRTOBJFILES ?= $(SEL4_LIBDIR)/crt1.o $(SEL4_LIBDIR)/crti.o
FINOBJFILES ?= $(SEL4_LIBDIR)/crtn.o

# Look away now
${MAINOBJ}: ${INTERMEDIATE_BASEFILE}
	echo "Building: $(MAINOBJ): $(CFLAGS)"
	# Generate the rumprun.o object file that is linked against apps that want to use rumprun.
	# --whole-archive is used to ensure constructor symbols are included from some libraries
	# --start-group because seL4 libraries have circular dependencies
	${CC} -nostdlib ${CFLAGS} ${CRTOBJFILES} ${INTERMEDIATE_BASEFILE} ${FINOBJFILES} -o $@ ${MAINOBJ_LD_FLAGS}
	# Objcopy is used to promote all -G symbols to global symbols while hiding all other symbols.
	${BASEFILE_OBJCOPY} $@

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
