



ifeq ($(CC),cc)
CC := $(CROSS_COMPILE)gcc
endif

CFLAGS += -mbranch-protection=pac-ret

pauth_cc_support := $(shell if ($(CC) $(CFLAGS) -march=armv8.3-a -E -x c /dev/null -o /dev/null 2>&1) then echo "1"; fi)

ifeq ($(pauth_cc_support),1)
TEST_GEN_PROGS := pac
TEST_GEN_FILES := pac_corruptor.o helper.o
TEST_GEN_PROGS_EXTENDED := exec_target
endif

include ../../lib.mk

ifeq ($(pauth_cc_support),1)


$(OUTPUT)/pac_corruptor.o: pac_corruptor.S
	$(CC) -c $^ -o $@ $(CFLAGS) -march=armv8.3-a

$(OUTPUT)/helper.o: helper.c
	$(CC) -c $^ -o $@ $(CFLAGS) -march=armv8.3-a





$(OUTPUT)/exec_target: exec_target.c $(OUTPUT)/helper.o
	$(CC) $^ -o $@ $(CFLAGS) -march=armv8.2-a

$(OUTPUT)/pac: pac.c $(OUTPUT)/pac_corruptor.o $(OUTPUT)/helper.o
	$(CC) $^ -o $@ $(CFLAGS) -march=armv8.2-a
endif
