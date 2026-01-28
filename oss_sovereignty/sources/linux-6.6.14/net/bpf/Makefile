
obj-$(CONFIG_BPF_SYSCALL)	:= test_run.o
ifeq ($(CONFIG_BPF_JIT),y)
obj-$(CONFIG_BPF_SYSCALL)	+= bpf_dummy_struct_ops.o
endif
