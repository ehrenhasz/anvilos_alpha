include Kbuild

INSTALL_MOD_DIR ?= extra
INSTALL_MOD_PATH ?= $(DESTDIR)

all: modules
distclean maintainer-clean: clean
install: modules_install data_install
uninstall: modules_uninstall data_uninstall
check:

.PHONY: all distclean maintainer-clean install uninstall check distdir \
	modules modules-Linux modules-FreeBSD modules-unknown \
	clean clean-Linux clean-FreeBSD \
	modules_install modules_install-Linux modules_install-FreeBSD \
	data_install data_install-Linux data_install-FreeBSD \
	modules_uninstall modules_uninstall-Linux modules_uninstall-FreeBSD \
	data_uninstall data_uninstall-Linux data_uninstall-FreeBSD \
	cppcheck cppcheck-Linux cppcheck-FreeBSD


export WITH_DEBUG ?= 
export WITH_INVARIANTS ?= 


getflags = ( \
set -- \
  $(filter-out --%,$(firstword $(MFLAGS))) \
  $(filter -I%,$(MFLAGS)) \
  $(filter -j%,$(MFLAGS)); \
fmakeflags=""; \
while getopts :deiI:j:knqrstw flag; do \
  case $$flag in \
    \?) :;; \
    :) if [ $$OPTARG = "j" ]; then \
	 ncpus=$$(sysctl -n kern.smp.cpus 2>/dev/null || :); \
	 if [ -n "$$ncpus" ]; then fmakeflags="$$fmakeflags -j$$ncpus"; fi; \
       fi;; \
    d) fmakeflags="$$fmakeflags -dA";; \
    *) fmakeflags="$$fmakeflags -$$flag$$OPTARG";; \
  esac; \
done; \
echo $$fmakeflags \
)
FMAKEFLAGS = -C /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module -f Makefile.bsd $(shell $(getflags))

ifneq (/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module,/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module)
FMAKEFLAGS += MAKEOBJDIR=/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module
endif

FMAKE = env -u MAKEFLAGS make $(FMAKEFLAGS)

modules-Linux:
	mkdir -p $(sort $(dir $(spl-objs) $(spl-)))
	mkdir -p $(sort $(dir $(zfs-objs) $(zfs-)))
	$(MAKE) -C /home/aimeat/anvilos/oss_sovereignty/linux-6.6.14 $(if ,CC=) \
		$(if ,LD=) $(if ,LLVM=) \
		M="$$PWD"  CONFIG_ZFS=m modules

modules-FreeBSD:
	+$(FMAKE)

modules-unknown:
	@true

modules: modules-Linux

clean-Linux:
	@
	@
	$(MAKE) -C /home/aimeat/anvilos/oss_sovereignty/linux-6.6.14 M="$$PWD"  clean

	$(RM) Module.symvers Module.markers
	find . -name '*.ur-safe' -type f -delete

clean-FreeBSD:
	+$(FMAKE) clean

clean: clean-Linux

.PHONY: modules_uninstall-Linux-legacy
modules_uninstall-Linux-legacy:
	$(RM) -r $(addprefix $(KMODDIR)/$(INSTALL_MOD_DIR)/,spl/ avl/ icp/ lua/ nvpair/ unicode/ zcommon/ zfs/ zstd/)

KMODDIR := $(INSTALL_MOD_PATH)/lib/modules/6.6.14
modules_install-Linux: modules_uninstall-Linux-legacy
	@
	$(MAKE) -C /home/aimeat/anvilos/oss_sovereignty/linux-6.6.14 M="$$PWD" modules_install \
		INSTALL_MOD_PATH=$(INSTALL_MOD_PATH) \
		INSTALL_MOD_DIR=$(INSTALL_MOD_DIR) \
		KERNELRELEASE=6.6.14
	@
	if [ -n "$(DESTDIR)" ]; then \
		find $(KMODDIR) -name 'modules.*' -delete; \
	fi
	@
	@
	@
	@
	@
	@
	sysmap=$(INSTALL_MOD_PATH)/boot/System.map-6.6.14; \
	{ [ -f "$$sysmap" ] && [ $$(wc -l < "$$sysmap") -ge 100 ]; } || \
		sysmap=$(INSTALL_MOD_PATH)/usr/lib/debug/boot/System.map-6.6.14; \
	if [ -f $$sysmap ]; then \
		depmod -ae -F $$sysmap 6.6.14; \
	fi

modules_install-FreeBSD:
	@
	+$(FMAKE) install

modules_install: modules_install-Linux

data_install-Linux:
	@mkdir -p $(DESTDIR)//usr/local/src/zfs-2.2.2/6.6.14
	cp ../zfs.release ../zfs_config.h Module.symvers $(DESTDIR)//usr/local/src/zfs-2.2.2/6.6.14

data_install-FreeBSD:
	@

data_install: data_install-Linux

modules_uninstall-Linux: modules_uninstall-Linux-legacy
	@
	$(RM) $(addprefix $(KMODDIR)/$(INSTALL_MOD_DIR)/,zfs.ko spl.ko)

modules_uninstall-FreeBSD:
	@false

modules_uninstall: modules_uninstall-Linux

data_uninstall-Linux:
	$(RM) $(addprefix $(DESTDIR)//usr/local/src/zfs-2.2.2/6.6.14/,zfs.release zfs_config.h Module.symvers)

data_uninstall-FreeBSD:
	@

data_uninstall: data_uninstall-Linux

cppcheck-Linux:
	@printf "skipping cppcheck because cppcheck is not installed\n" -j8 --std=c99 --quiet --force --error-exitcode=2 \
		--inline-suppr \
		--suppress=unmatchedSuppression \
		--suppress=noValidConfiguration \
		--enable=warning,information -D_KERNEL \
		--include=/home/aimeat/anvilos/oss_sovereignty/linux-6.6.14/include/generated/autoconf.h \
		--include=../zfs_config.h \
		--config-exclude=/home/aimeat/anvilos/oss_sovereignty/linux-6.6.14/include \
		-i zstd/lib \
		-I /home/aimeat/anvilos/oss_sovereignty/linux-6.6.14/include \
		-I ../include/os/linux/kernel \
		-I ../include/os/linux/spl \
		-I ../include/os/linux/zfs \
		-I ../include \
		avl icp lua nvpair unicode zcommon zfs zstd os/linux

cppcheck-FreeBSD:
	@true

cppcheck: cppcheck-Linux

distdir:
	cd . && find . -name '*.[chS]' -exec sh -c 'for f; do mkdir -p $$distdir/$${f%/*}; cp ./$$f $$distdir/$$f; done' _ {} +
	cp ./Makefile.bsd $$distdir/Makefile.bsd

gen-zstd-symbols:
	for obj in $(addprefix zstd/,$(ZSTD_UPSTREAM_OBJS)); do echo; echo "/* $${obj

check-zstd-symbols:
	objdump -t $(addprefix zstd/,$(ZSTD_UPSTREAM_OBJS)) | awk '/file format/ {print}  $$2 == "g" && (!/ zfs_/ && !/ __pfx_zfs_/) {++ret; print}  END {exit ret}'
