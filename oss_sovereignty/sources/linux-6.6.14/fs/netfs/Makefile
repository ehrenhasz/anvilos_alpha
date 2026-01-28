

netfs-y := \
	buffered_read.o \
	io.o \
	iterator.o \
	main.o \
	objects.o

netfs-$(CONFIG_NETFS_STATS) += stats.o

obj-$(CONFIG_NETFS_SUPPORT) := netfs.o
