



CFLAGS += -Wall -O2 $(KHDR_INCLUDES)

LOCAL_HDRS += common.h

src_test := $(wildcard *_test.c)

TEST_GEN_PROGS := $(src_test:.c=)

TEST_GEN_PROGS_EXTENDED := true


$(TEST_GEN_PROGS): LDLIBS += -lcap
$(TEST_GEN_PROGS_EXTENDED): LDFLAGS += -static

include ../lib.mk


$(TEST_GEN_PROGS): LDLIBS += -lcap
$(TEST_GEN_PROGS_EXTENDED): LDFLAGS += -static
