
ccflags-$(CONFIG_PPC_WERROR)	+= -Werror

ocxl-y				+= main.o pci.o config.o file.o pasid.o mmio.o
ocxl-y				+= link.o context.o afu_irq.o sysfs.o trace.o
ocxl-y				+= core.o
obj-$(CONFIG_OCXL)		+= ocxl.o


CFLAGS_trace.o := -I$(src)


