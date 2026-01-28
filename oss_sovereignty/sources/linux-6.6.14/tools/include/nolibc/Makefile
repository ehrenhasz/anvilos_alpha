

include ../../scripts/Makefile.include


ifeq ($(srctree),)
srctree := $(patsubst %/tools/include/,%,$(dir $(CURDIR)))
endif


ifeq ($(ARCH),)
include $(srctree)/scripts/subarch.include
ARCH = $(SUBARCH)
endif



OUTPUT ?= $(CURDIR)/

ifeq ($(V),1)
Q=
else
Q=@
endif

nolibc_arch := $(patsubst arm64,aarch64,$(ARCH))
arch_file := arch-$(nolibc_arch).h
all_files := \
		compiler.h \
		crt.h \
		ctype.h \
		errno.h \
		nolibc.h \
		signal.h \
		stackprotector.h \
		std.h \
		stdint.h \
		stdlib.h \
		string.h \
		sys.h \
		time.h \
		types.h \
		unistd.h \
		stdio.h \



all: headers

install: help

help:
	@echo "Supported targets under nolibc:"
	@echo "  all                 call \"headers\""
	@echo "  clean               clean the sysroot"
	@echo "  headers             prepare a sysroot in tools/include/nolibc/sysroot"
	@echo "  headers_standalone  like \"headers\", and also install kernel headers"
	@echo "  help                this help"
	@echo ""
	@echo "These targets may also be called from tools as \"make nolibc_<target>\"."
	@echo ""
	@echo "Currently using the following variables:"
	@echo "  ARCH    = $(ARCH)"
	@echo "  OUTPUT  = $(OUTPUT)"
	@echo ""


headers:
	$(Q)mkdir -p $(OUTPUT)sysroot
	$(Q)mkdir -p $(OUTPUT)sysroot/include
	$(Q)cp $(all_files) $(OUTPUT)sysroot/include/
	$(Q)if [ "$(ARCH)" = "x86" ]; then      \
		sed -e                          \
		  's,^
		  arch-x86_64.h;                \
		sed -e                          \
		  's,^
		  arch-i386.h;                  \
	elif [ -e "$(arch_file)" ]; then        \
		cat $(arch_file);               \
	else                                    \
		echo "Fatal: architecture $(ARCH) not yet supported by nolibc." >&2; \
		exit 1;                         \
	fi > $(OUTPUT)sysroot/include/arch.h

headers_standalone: headers
	$(Q)$(MAKE) -C $(srctree) headers
	$(Q)$(MAKE) -C $(srctree) headers_install INSTALL_HDR_PATH=$(OUTPUT)sysroot

clean:
	$(call QUIET_CLEAN, nolibc) rm -rf "$(OUTPUT)sysroot"
