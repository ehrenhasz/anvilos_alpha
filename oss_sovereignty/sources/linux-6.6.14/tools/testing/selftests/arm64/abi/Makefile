


TEST_GEN_PROGS := hwcap ptrace syscall-abi tpidr2

include ../../lib.mk

$(OUTPUT)/syscall-abi: syscall-abi.c syscall-abi-asm.S



$(OUTPUT)/tpidr2: tpidr2.c
	$(CC) -fno-asynchronous-unwind-tables -fno-ident -s -Os -nostdlib \
		-static -include ../../../../include/nolibc/nolibc.h \
		-ffreestanding -Wall $^ -o $@ -lgcc
