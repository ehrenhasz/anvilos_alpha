
CFLAGS += -Wl,-no-as-needed -Wall $(KHDR_INCLUDES)
LDFLAGS += -lpthread
LDLIBS += -lcap

TEST_GEN_PROGS := seccomp_bpf seccomp_benchmark
include ../lib.mk
