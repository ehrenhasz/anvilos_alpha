



TEST_GEN_PROGS := hwprobe

include ../../lib.mk

$(OUTPUT)/hwprobe: hwprobe.c sys_hwprobe.S
	$(CC) -o$@ $(CFLAGS) $(LDFLAGS) $^
