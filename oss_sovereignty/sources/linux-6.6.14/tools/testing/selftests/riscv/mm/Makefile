




CFLAGS += -D_GNU_SOURCE -std=gnu99 -I.

TEST_GEN_FILES := mmap_default mmap_bottomup

TEST_PROGS := run_mmap.sh

include ../../lib.mk

$(OUTPUT)/mm: mmap_default.c mmap_bottomup.c mmap_tests.h
	$(CC) -o$@ $(CFLAGS) $(LDFLAGS) $^
