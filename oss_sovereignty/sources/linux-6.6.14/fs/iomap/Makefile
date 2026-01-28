





ccflags-y += -I $(srctree)/$(src)		

obj-$(CONFIG_FS_IOMAP)		+= iomap.o

iomap-y				+= trace.o \
				   iter.o
iomap-$(CONFIG_BLOCK)		+= buffered-io.o \
				   direct-io.o \
				   fiemap.o \
				   seek.o
iomap-$(CONFIG_SWAP)		+= swapfile.o
