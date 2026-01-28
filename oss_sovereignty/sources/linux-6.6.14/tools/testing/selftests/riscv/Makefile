



ARCH ?= $(shell uname -m 2>/dev/null || echo not)

ifneq (,$(filter $(ARCH),riscv))
RISCV_SUBTARGETS ?= hwprobe vector mm
else
RISCV_SUBTARGETS :=
endif

CFLAGS := -Wall -O2 -g


top_srcdir = $(realpath ../../../../)


CFLAGS += -I$(top_srcdir)/tools/testing/selftests/

CFLAGS += $(KHDR_INCLUDES)

export CFLAGS
export top_srcdir

all:
	@for DIR in $(RISCV_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		mkdir -p $$BUILD_TARGET;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

install: all
	@for DIR in $(RISCV_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

run_tests: all
	@for DIR in $(RISCV_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done


emit_tests:
	@for DIR in $(RISCV_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

clean:
	@for DIR in $(RISCV_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

.PHONY: all clean install run_tests emit_tests
