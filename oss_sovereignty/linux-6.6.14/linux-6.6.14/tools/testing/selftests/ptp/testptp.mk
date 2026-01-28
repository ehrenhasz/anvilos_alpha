CC        = $(CROSS_COMPILE)gcc
INC       = -I$(KBUILD_OUTPUT)/usr/include
CFLAGS    = -Wall $(INC)
LDLIBS    = -lrt
PROGS     = testptp
all: $(PROGS)
testptp: testptp.o
clean:
	rm -f testptp.o
distclean: clean
	rm -f $(PROGS)
