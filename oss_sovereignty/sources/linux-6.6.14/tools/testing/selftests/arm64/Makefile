


ARCH ?= $(shell uname -m 2>/dev/null || echo not)

ifneq (,$(filter $(ARCH),aarch64 arm64))
ARM64_SUBTARGETS ?= tags signal pauth fp mte bti abi
else
ARM64_SUBTARGETS :=
endif

CFLAGS := -Wall -O2 -g


top_srcdir = $(realpath ../../../../)


CFLAGS += -I$(top_srcdir)/tools/testing/selftests/

CFLAGS += $(KHDR_INCLUDES)

CFLAGS += -I$(top_srcdir)/tools/include

export CFLAGS
export top_srcdir

all:
	@for DIR in $(ARM64_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		mkdir -p $$BUILD_TARGET;			\
		make OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

install: all
	@for DIR in $(ARM64_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		make OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

run_tests: all
	@for DIR in $(ARM64_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		make OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done


emit_tests:
	@for DIR in $(ARM64_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		make OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

clean:
	@for DIR in $(ARM64_SUBTARGETS); do				\
		BUILD_TARGET=$(OUTPUT)/$$DIR;			\
		make OUTPUT=$$BUILD_TARGET -C $$DIR $@;		\
	done

.PHONY: all clean install run_tests emit_tests
