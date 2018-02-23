ifeq ($(RUMPRUN_MKCONF),)
$(error RUMPRUN_MKCONF missing)
endif
include ${RUMPRUN_MKCONF}
ifeq (${RRDEST},)
$(error invalid RUMPRUN_MKCONF)
endif

# Suppress make from printing commands if BUILD_QUIET is set
ifeq (${BUILD_QUIET}, -qq)
	MAKE_SILENT = -s
else
ifeq (${BUILD_QUIET}, -q)
	MAKE_SILENT = -s
else
	MAKE_SILENT =
endif
endif

DBG?=	 -O2 -g
CFLAGS+= -std=gnu99 ${DBG}
CFLAGS+= -fno-stack-protector -ffreestanding
CXXFLAGS+= -fno-stack-protector -ffreestanding

CFLAGS+= -Wall -Wimplicit -Wmissing-prototypes -Wstrict-prototypes
ifndef NOGCCERROR
CFLAGS+= -Werror
endif

LDFLAGS.x86_64.hw= -z max-page-size=0x1000

ifeq (${BUILDRR},true)
INSTALLDIR=     ${RROBJ}/dest.stage
else
INSTALLDIR=     ${RRDEST}
endif

cc-option = $(shell if [ -z "`echo 'int p=1;' | $(CC) $(1) -S -o /dev/null -x c - 2>&1`" ]; \
                       then echo y; else echo n; fi)
