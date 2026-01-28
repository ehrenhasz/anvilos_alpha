



CFLAGS_cpu.o	:= -fno-stack-protector



CFLAGS_REMOVE_cpu.o := $(CC_FLAGS_LTO)

obj-$(CONFIG_PM_SLEEP)		+= cpu.o
obj-$(CONFIG_HIBERNATION)	+= hibernate_$(BITS).o hibernate_asm_$(BITS).o hibernate.o
