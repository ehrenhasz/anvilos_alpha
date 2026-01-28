




userprogs := bpfilter_umh
bpfilter_umh-objs := main.o
userccflags += -I $(srctree)/tools/include/ -I $(srctree)/tools/include/uapi

ifeq ($(CONFIG_BPFILTER_UMH), y)



userldflags += -static
endif

$(obj)/bpfilter_umh_blob.o: $(obj)/bpfilter_umh

obj-$(CONFIG_BPFILTER_UMH) += bpfilter.o
bpfilter-objs += bpfilter_kern.o bpfilter_umh_blob.o
