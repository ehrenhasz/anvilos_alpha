
TARGETS += alsa
TARGETS += amd-pstate
TARGETS += arm64
TARGETS += bpf
TARGETS += breakpoints
TARGETS += cachestat
TARGETS += capabilities
TARGETS += cgroup
TARGETS += clone3
TARGETS += connector
TARGETS += core
TARGETS += cpufreq
TARGETS += cpu-hotplug
TARGETS += damon
TARGETS += dmabuf-heaps
TARGETS += drivers/dma-buf
TARGETS += drivers/s390x/uvdevice
TARGETS += drivers/net/bonding
TARGETS += drivers/net/team
TARGETS += efivarfs
TARGETS += exec
TARGETS += fchmodat2
TARGETS += filesystems
TARGETS += filesystems/binderfs
TARGETS += filesystems/epoll
TARGETS += filesystems/fat
TARGETS += firmware
TARGETS += fpu
TARGETS += ftrace
TARGETS += futex
TARGETS += gpio
TARGETS += hid
TARGETS += intel_pstate
TARGETS += iommu
TARGETS += ipc
TARGETS += ir
TARGETS += kcmp
TARGETS += kexec
TARGETS += kvm
TARGETS += landlock
TARGETS += lib
TARGETS += livepatch
TARGETS += lkdtm
TARGETS += membarrier
TARGETS += memfd
TARGETS += memory-hotplug
TARGETS += mincore
TARGETS += mount
TARGETS += mount_setattr
TARGETS += move_mount_set_group
TARGETS += mqueue
TARGETS += nci
TARGETS += net
TARGETS += net/af_unix
TARGETS += net/forwarding
TARGETS += net/hsr
TARGETS += net/mptcp
TARGETS += net/openvswitch
TARGETS += netfilter
TARGETS += nsfs
TARGETS += perf_events
TARGETS += pidfd
TARGETS += pid_namespace
TARGETS += powerpc
TARGETS += prctl
TARGETS += proc
TARGETS += pstore
TARGETS += ptrace
TARGETS += openat2
TARGETS += resctrl
TARGETS += riscv
TARGETS += rlimits
TARGETS += rseq
TARGETS += rtc
TARGETS += seccomp
TARGETS += sgx
TARGETS += sigaltstack
TARGETS += size
TARGETS += sparc64
TARGETS += splice
TARGETS += static_keys
TARGETS += sync
TARGETS += syscall_user_dispatch
TARGETS += sysctl
TARGETS += tc-testing
TARGETS += tdx
TARGETS += timens
ifneq (1, $(quicktest))
TARGETS += timers
endif
TARGETS += tmpfs
TARGETS += tpm2
TARGETS += tty
TARGETS += uevent
TARGETS += user
TARGETS += user_events
TARGETS += vDSO
TARGETS += mm
TARGETS += x86
TARGETS += zram




TARGETS_HOTPLUG = cpu-hotplug
TARGETS_HOTPLUG += memory-hotplug




SKIP_TARGETS ?= bpf
ifneq ($(SKIP_TARGETS),)
	TMP := $(filter-out $(SKIP_TARGETS), $(TARGETS))
	override TARGETS := $(TMP)
endif





FORCE_TARGETS ?=




ifeq (,$(LINK.c))
override LDFLAGS =
override MAKEFLAGS =
endif




ifdef building_out_of_srctree
override LDFLAGS =
endif

top_srcdir ?= ../../..

ifeq ("$(origin O)", "command line")
  KBUILD_OUTPUT := $(O)
endif

ifneq ($(KBUILD_OUTPUT),)
  
  
  abs_objtree := $(shell cd $(top_srcdir) && mkdir -p $(KBUILD_OUTPUT) && cd $(KBUILD_OUTPUT) && pwd)
  $(if $(abs_objtree),, \
    $(error failed to create output directory "$(KBUILD_OUTPUT)"))
  
  abs_objtree := $(realpath $(abs_objtree))
  BUILD := $(abs_objtree)/kselftest
  KHDR_INCLUDES := -isystem ${abs_objtree}/usr/include
else
  BUILD := $(CURDIR)
  abs_srctree := $(shell cd $(top_srcdir) && pwd)
  KHDR_INCLUDES := -isystem ${abs_srctree}/usr/include
  DEFAULT_INSTALL_HDR_PATH := 1
endif


include $(top_srcdir)/scripts/subarch.include
ARCH           ?= $(SUBARCH)
export BUILD
export KHDR_INCLUDES



.DEFAULT_GOAL := all

all:
	@ret=1;							\
	for TARGET in $(TARGETS); do				\
		BUILD_TARGET=$$BUILD/$$TARGET;			\
		mkdir $$BUILD_TARGET  -p;			\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET	\
				O=$(abs_objtree)		\
				$(if $(FORCE_TARGETS),|| exit);	\
		ret=$$((ret * $$?));				\
	done; exit $$ret;

run_tests: all
	@for TARGET in $(TARGETS); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET run_tests \
				O=$(abs_objtree);		    \
	done;

hotplug:
	@for TARGET in $(TARGETS_HOTPLUG); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET;\
	done;

run_hotplug: hotplug
	@for TARGET in $(TARGETS_HOTPLUG); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET run_full_test;\
	done;

clean_hotplug:
	@for TARGET in $(TARGETS_HOTPLUG); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET clean;\
	done;

run_pstore_crash:
	$(MAKE) -C pstore run_crash








KSFT_INSTALL_PATH ?= $(BUILD)/kselftest_install
KSFT_INSTALL_PATH := $(abspath $(KSFT_INSTALL_PATH))

INSTALL_PATH := $(KSFT_INSTALL_PATH)
ALL_SCRIPT := $(INSTALL_PATH)/run_kselftest.sh
TEST_LIST := $(INSTALL_PATH)/kselftest-list.txt

install: all
ifdef INSTALL_PATH
	@
	mkdir -p $(INSTALL_PATH)/kselftest
	install -m 744 kselftest/module.sh $(INSTALL_PATH)/kselftest/
	install -m 744 kselftest/runner.sh $(INSTALL_PATH)/kselftest/
	install -m 744 kselftest/prefix.pl $(INSTALL_PATH)/kselftest/
	install -m 744 run_kselftest.sh $(INSTALL_PATH)/
	rm -f $(TEST_LIST)
	@ret=1;	\
	for TARGET in $(TARGETS); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET INSTALL_PATH=$(INSTALL_PATH)/$$TARGET install \
				O=$(abs_objtree)		\
				$(if $(FORCE_TARGETS),|| exit);	\
		ret=$$((ret * $$?));		\
	done; exit $$ret;


	@
	@
	@
	@
	for TARGET in $(TARGETS); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		[ ! -d $(INSTALL_PATH)/$$TARGET ] && printf "Skipping non-existent dir: $$TARGET\n" && continue; \
		printf "Emit Tests for $$TARGET\n"; \
		$(MAKE) -s --no-print-directory OUTPUT=$$BUILD_TARGET COLLECTION=$$TARGET \
			-C $$TARGET emit_tests >> $(TEST_LIST); \
	done;
else
	$(error Error: set INSTALL_PATH to use install)
endif

FORMAT ?= .gz
TAR_PATH = $(abspath ${INSTALL_PATH}/kselftest-packages/kselftest.tar${FORMAT})
gen_tar: install
	@mkdir -p ${INSTALL_PATH}/kselftest-packages/
	@tar caf ${TAR_PATH} --exclude=kselftest-packages -C ${INSTALL_PATH} .
	@echo "Created ${TAR_PATH}"

clean:
	@for TARGET in $(TARGETS); do \
		BUILD_TARGET=$$BUILD/$$TARGET;	\
		$(MAKE) OUTPUT=$$BUILD_TARGET -C $$TARGET clean;\
	done;

.PHONY: all run_tests hotplug run_hotplug clean_hotplug run_pstore_crash install clean gen_tar
