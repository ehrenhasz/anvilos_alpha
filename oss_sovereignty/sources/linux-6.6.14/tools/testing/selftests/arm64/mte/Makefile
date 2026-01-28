


CFLAGS += -std=gnu99 -I. -pthread
LDFLAGS += -pthread
SRCS := $(filter-out mte_common_util.c,$(wildcard *.c))
PROGS := $(patsubst %.c,%,$(SRCS))

ifeq ($(LLVM),)



ifeq ($(CC),cc)
CC := $(CROSS_COMPILE)gcc
endif


mte_cc_support := $(shell if ($(CC) $(CFLAGS) -march=armv8.5-a+memtag -E -x c /dev/null -o /dev/null 2>&1) then echo "1"; fi)

else


mte_cc_support := 1

endif

ifeq ($(mte_cc_support),1)

TEST_GEN_PROGS := $(PROGS)

else
    $(warning compiler "$(CC)" does not support the ARMv8.5 MTE extension.)
    $(warning test program "mte" will not be created.)
endif


include ../../lib.mk

ifeq ($(mte_cc_support),1)
$(TEST_GEN_PROGS): mte_common_util.c mte_helper.S
endif
