



CFLAGS += -D_GNU_SOURCE -std=gnu99 -I.

SRCS := $(filter-out testcases/testcases.c,$(wildcard testcases/*.c))
PROGS := $(patsubst %.c,%,$(SRCS))


TEST_GEN_PROGS := $(notdir $(PROGS))






include ../../lib.mk

$(TEST_GEN_PROGS): $(PROGS)
	cp $(PROGS) $(OUTPUT)/



COMMON_SOURCES := test_signals.c test_signals_utils.c testcases/testcases.c \
	signals.S
COMMON_HEADERS := test_signals.h test_signals_utils.h testcases/testcases.h

.SECONDEXPANSION:
$(PROGS): $$@.c ${COMMON_SOURCES} ${COMMON_HEADERS}
	$(CC) $(CFLAGS) ${@}.c ${COMMON_SOURCES} -o $@
