


include ../../../build/Build.include
include ../../../scripts/Makefile.arch
include ../../../scripts/Makefile.include

TEST_PROGS := hid-core.sh
TEST_PROGS += hid-apple.sh
TEST_PROGS += hid-gamepad.sh
TEST_PROGS += hid-ite.sh
TEST_PROGS += hid-keyboard.sh
TEST_PROGS += hid-mouse.sh
TEST_PROGS += hid-multitouch.sh
TEST_PROGS += hid-sony.sh
TEST_PROGS += hid-tablet.sh
TEST_PROGS += hid-usb_crash.sh
TEST_PROGS += hid-wacom.sh

CXX ?= $(CROSS_COMPILE)g++

HOSTPKG_CONFIG := pkg-config

CFLAGS += -g -O0 -rdynamic -Wall -Werror -I$(KHDR_INCLUDES) -I$(OUTPUT)
LDLIBS += -lelf -lz -lrt -lpthread


ifneq ($(LLVM),)
CFLAGS += -Wno-unused-command-line-argument
endif


TEST_GEN_PROGS = hid_bpf






ifeq ($(V),1)
Q =
msg =
else
Q = @
msg = @printf '  %-8s%s %s%s\n' "$(1)" "$(if $(2), [$(2)])" "$(notdir $(3))" "$(if $(4), $(4))";
MAKEFLAGS += --no-print-directory
submake_extras := feature_display=0
endif


OVERRIDE_TARGETS := 1
override define CLEAN
	$(call msg,CLEAN)
	$(Q)$(RM) -r $(TEST_GEN_PROGS)
	$(Q)$(RM) -r $(EXTRA_CLEAN)
endef

include ../lib.mk

TOOLSDIR := $(top_srcdir)/tools
LIBDIR := $(TOOLSDIR)/lib
BPFDIR := $(LIBDIR)/bpf
TOOLSINCDIR := $(TOOLSDIR)/include
BPFTOOLDIR := $(TOOLSDIR)/bpf/bpftool
SCRATCH_DIR := $(OUTPUT)/tools
BUILD_DIR := $(SCRATCH_DIR)/build
INCLUDE_DIR := $(SCRATCH_DIR)/include
KHDR_INCLUDES := $(SCRATCH_DIR)/uapi/include
BPFOBJ := $(BUILD_DIR)/libbpf/libbpf.a
ifneq ($(CROSS_COMPILE),)
HOST_BUILD_DIR		:= $(BUILD_DIR)/host
HOST_SCRATCH_DIR	:= $(OUTPUT)/host-tools
HOST_INCLUDE_DIR	:= $(HOST_SCRATCH_DIR)/include
else
HOST_BUILD_DIR		:= $(BUILD_DIR)
HOST_SCRATCH_DIR	:= $(SCRATCH_DIR)
HOST_INCLUDE_DIR	:= $(INCLUDE_DIR)
endif
HOST_BPFOBJ := $(HOST_BUILD_DIR)/libbpf/libbpf.a
RESOLVE_BTFIDS := $(HOST_BUILD_DIR)/resolve_btfids/resolve_btfids

VMLINUX_BTF_PATHS ?= $(if $(O),$(O)/vmlinux)				\
		     $(if $(KBUILD_OUTPUT),$(KBUILD_OUTPUT)/vmlinux)	\
		     ../../../../vmlinux				\
		     /sys/kernel/btf/vmlinux				\
		     /boot/vmlinux-$(shell uname -r)
VMLINUX_BTF ?= $(abspath $(firstword $(wildcard $(VMLINUX_BTF_PATHS))))
ifeq ($(VMLINUX_BTF),)
$(error Cannot find a vmlinux for VMLINUX_BTF at any of "$(VMLINUX_BTF_PATHS)")
endif





$(notdir $(TEST_GEN_PROGS)): %: $(OUTPUT)/% ;


MAKE_DIRS := $(sort $(BUILD_DIR)/libbpf $(HOST_BUILD_DIR)/libbpf		\
	       $(HOST_BUILD_DIR)/bpftool $(HOST_BUILD_DIR)/resolve_btfids	\
	       $(INCLUDE_DIR))
$(MAKE_DIRS):
	$(call msg,MKDIR,,$@)
	$(Q)mkdir -p $@


ifeq ($(SRCARCH),x86)
LLD := lld
else
LLD := ld
endif

DEFAULT_BPFTOOL := $(HOST_SCRATCH_DIR)/sbin/bpftool

TEST_GEN_PROGS_EXTENDED += $(DEFAULT_BPFTOOL)

$(TEST_GEN_PROGS) $(TEST_GEN_PROGS_EXTENDED): $(BPFOBJ)

BPFTOOL ?= $(DEFAULT_BPFTOOL)
$(DEFAULT_BPFTOOL): $(wildcard $(BPFTOOLDIR)/*.[ch] $(BPFTOOLDIR)/Makefile)    \
		    $(HOST_BPFOBJ) | $(HOST_BUILD_DIR)/bpftool
	$(Q)$(MAKE) $(submake_extras)  -C $(BPFTOOLDIR)			       \
		    ARCH= CROSS_COMPILE= CC=$(HOSTCC) LD=$(HOSTLD) 	       \
		    EXTRA_CFLAGS='-g -O0'				       \
		    OUTPUT=$(HOST_BUILD_DIR)/bpftool/			       \
		    LIBBPF_OUTPUT=$(HOST_BUILD_DIR)/libbpf/		       \
		    LIBBPF_DESTDIR=$(HOST_SCRATCH_DIR)/			       \
		    prefix= DESTDIR=$(HOST_SCRATCH_DIR)/ install-bin

$(BPFOBJ): $(wildcard $(BPFDIR)/*.[ch] $(BPFDIR)/Makefile)		       \
	   | $(BUILD_DIR)/libbpf
	$(Q)$(MAKE) $(submake_extras) -C $(BPFDIR) OUTPUT=$(BUILD_DIR)/libbpf/ \
		    EXTRA_CFLAGS='-g -O0'				       \
		    DESTDIR=$(SCRATCH_DIR) prefix= all install_headers

ifneq ($(BPFOBJ),$(HOST_BPFOBJ))
$(HOST_BPFOBJ): $(wildcard $(BPFDIR)/*.[ch] $(BPFDIR)/Makefile)		       \
		| $(HOST_BUILD_DIR)/libbpf
	$(Q)$(MAKE) $(submake_extras) -C $(BPFDIR)                             \
		    EXTRA_CFLAGS='-g -O0' ARCH= CROSS_COMPILE=		       \
		    OUTPUT=$(HOST_BUILD_DIR)/libbpf/ CC=$(HOSTCC) LD=$(HOSTLD) \
		    DESTDIR=$(HOST_SCRATCH_DIR)/ prefix= all install_headers
endif

$(INCLUDE_DIR)/vmlinux.h: $(VMLINUX_BTF) $(BPFTOOL) | $(INCLUDE_DIR)
ifeq ($(VMLINUX_H),)
	$(call msg,GEN,,$@)
	$(Q)$(BPFTOOL) btf dump file $(VMLINUX_BTF) format c > $@
else
	$(call msg,CP,,$@)
	$(Q)cp "$(VMLINUX_H)" $@
endif

$(KHDR_INCLUDES)/linux/hid.h: $(top_srcdir)/include/uapi/linux/hid.h
	$(MAKE) -C $(top_srcdir) INSTALL_HDR_PATH=$(SCRATCH_DIR)/uapi headers_install

$(RESOLVE_BTFIDS): $(HOST_BPFOBJ) | $(HOST_BUILD_DIR)/resolve_btfids	\
		       $(TOOLSDIR)/bpf/resolve_btfids/main.c	\
		       $(TOOLSDIR)/lib/rbtree.c			\
		       $(TOOLSDIR)/lib/zalloc.c			\
		       $(TOOLSDIR)/lib/string.c			\
		       $(TOOLSDIR)/lib/ctype.c			\
		       $(TOOLSDIR)/lib/str_error_r.c
	$(Q)$(MAKE) $(submake_extras) -C $(TOOLSDIR)/bpf/resolve_btfids	\
		CC=$(HOSTCC) LD=$(HOSTLD) AR=$(HOSTAR) \
		LIBBPF_INCLUDE=$(HOST_INCLUDE_DIR) \
		OUTPUT=$(HOST_BUILD_DIR)/resolve_btfids/ BPFOBJ=$(HOST_BPFOBJ)







define get_sys_includes
$(shell $(1) -v -E - </dev/null 2>&1 \
	| sed -n '/<...> search starts here:/,/End of search list./{ s| \(/.*\)|-idirafter \1|p }') \
$(shell $(1) -dM -E - </dev/null | grep '__riscv_xlen ' | awk '{printf("-D__riscv_xlen=%d -D__BITS_PER_LONG=%d", $$3, $$3)}')
endef


IS_LITTLE_ENDIAN = $(shell $(CC) -dM -E - </dev/null | \
			grep 'define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__')
MENDIAN=$(if $(IS_LITTLE_ENDIAN),-mlittle-endian,-mbig-endian)

CLANG_SYS_INCLUDES = $(call get_sys_includes,$(CLANG))
BPF_CFLAGS = -g -Werror -D__TARGET_ARCH_$(SRCARCH) $(MENDIAN) 		\
	     -I$(INCLUDE_DIR)

CLANG_CFLAGS = $(CLANG_SYS_INCLUDES) \
	       -Wno-compare-distinct-pointer-types





define CLANG_BPF_BUILD_RULE
	$(call msg,CLNG-BPF,$(TRUNNER_BINARY),$2)
	$(Q)$(CLANG) $3 -O2 --target=bpf -c $1 -mcpu=v3 -o $2
endef

define CLANG_NOALU32_BPF_BUILD_RULE
	$(call msg,CLNG-BPF,$(TRUNNER_BINARY),$2)
	$(Q)$(CLANG) $3 -O2 --target=bpf -c $1 -mcpu=v2 -o $2
endef

define GCC_BPF_BUILD_RULE
	$(call msg,GCC-BPF,$(TRUNNER_BINARY),$2)
	$(Q)$(BPF_GCC) $3 -O2 -c $1 -o $2
endef

BPF_PROGS_DIR := progs
BPF_BUILD_RULE := CLANG_BPF_BUILD_RULE
BPF_SRCS := $(notdir $(wildcard $(BPF_PROGS_DIR)/*.c))
BPF_OBJS := $(patsubst %.c,$(OUTPUT)/%.bpf.o, $(BPF_SRCS))
BPF_SKELS := $(patsubst %.c,$(OUTPUT)/%.skel.h, $(BPF_SRCS))
TEST_GEN_FILES += $(BPF_OBJS)

$(BPF_PROGS_DIR)-bpfobjs := y
$(BPF_OBJS): $(OUTPUT)/%.bpf.o:				\
	     $(BPF_PROGS_DIR)/%.c			\
	     $(wildcard $(BPF_PROGS_DIR)/*.h)		\
	     $(INCLUDE_DIR)/vmlinux.h				\
	     $(wildcard $(BPFDIR)/hid_bpf_*.h)			\
	     $(wildcard $(BPFDIR)/*.bpf.h)			\
	     | $(OUTPUT) $(BPFOBJ)
	$(call $(BPF_BUILD_RULE),$<,$@, $(BPF_CFLAGS))

$(BPF_SKELS): %.skel.h: %.bpf.o $(BPFTOOL) | $(OUTPUT)
	$(call msg,GEN-SKEL,$(BINARY),$@)
	$(Q)$(BPFTOOL) gen object $(<:.o=.linked1.o) $<
	$(Q)$(BPFTOOL) gen skeleton $(<:.o=.linked1.o) name $(notdir $(<:.bpf.o=)) > $@

$(OUTPUT)/%.o: %.c $(BPF_SKELS) $(KHDR_INCLUDES)/linux/hid.h
	$(call msg,CC,,$@)
	$(Q)$(CC) $(CFLAGS) -c $(filter %.c,$^) $(LDLIBS) -o $@

$(OUTPUT)/%: $(OUTPUT)/%.o
	$(call msg,BINARY,,$@)
	$(Q)$(LINK.c) $^ $(LDLIBS) -o $@

EXTRA_CLEAN := $(SCRATCH_DIR) $(HOST_SCRATCH_DIR) feature bpftool	\
	$(addprefix $(OUTPUT)/,*.o *.skel.h no_alu32)
