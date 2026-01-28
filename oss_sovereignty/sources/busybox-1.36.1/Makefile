VERSION = 1
PATCHLEVEL = 36
SUBLEVEL = 1
EXTRAVERSION =
NAME = Unnamed








MAKEFLAGS += --no-print-directory




















ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif




ifdef C
  ifeq ("$(origin C)", "command line")
    KBUILD_CHECKSRC = $(C)
  endif
endif
ifndef KBUILD_CHECKSRC
  KBUILD_CHECKSRC = 0
endif




ifdef SUBDIRS
  KBUILD_EXTMOD ?= $(SUBDIRS)
endif
ifdef M
  ifeq ("$(origin M)", "command line")
    KBUILD_EXTMOD := $(M)
  endif
endif




















ifeq ($(KBUILD_SRC),)



ifdef O
  ifeq ("$(origin O)", "command line")
    KBUILD_OUTPUT := $(O)
  endif
endif


PHONY := _all
_all:

ifneq ($(KBUILD_OUTPUT),)


saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell cd $(KBUILD_OUTPUT) && /bin/pwd)
$(if $(KBUILD_OUTPUT),, \
     $(error output directory "$(saved-output)" does not exist))

PHONY += $(MAKECMDGOALS)

$(filter-out _all,$(MAKECMDGOALS)) _all:
	$(if $(KBUILD_VERBOSE:1=),@)$(MAKE) -C $(KBUILD_OUTPUT) \
	KBUILD_SRC=$(CURDIR) \
	KBUILD_EXTMOD="$(KBUILD_EXTMOD)" -f $(CURDIR)/Makefile $@


skip-makefile := 1
endif 
endif 


ifeq ($(skip-makefile),)



PHONY += all
ifeq ($(KBUILD_EXTMOD),)
_all: all
else
_all: modules
endif

srctree		:= $(if $(KBUILD_SRC),$(KBUILD_SRC),$(CURDIR))
TOPDIR		:= $(srctree)

objtree		:= $(CURDIR)
src		:= $(srctree)
obj		:= $(objtree)

VPATH		:= $(srctree)$(if $(KBUILD_EXTMOD),:$(KBUILD_EXTMOD))

export srctree objtree VPATH TOPDIR





















CROSS_COMPILE ?=


ifeq ($(CROSS_COMPILE),)
CROSS_COMPILE := $(shell grep ^CONFIG_CROSS_COMPILER_PREFIX .config 2>/dev/null)
CROSS_COMPILE := $(subst CONFIG_CROSS_COMPILER_PREFIX=,,$(CROSS_COMPILE))
CROSS_COMPILE := $(subst ",,$(CROSS_COMPILE))

endif







ifneq ($(CROSS_COMPILE),)
SUBARCH := $(shell echo $(CROSS_COMPILE) | cut -d- -f1 | sed 's:^.*/::g')
else
SUBARCH := $(shell uname -m)
endif
SUBARCH := $(shell echo $(SUBARCH) | sed -e s/i.86/i386/ -e s/sun4u/sparc64/ \
					 -e s/arm.*/arm/ -e s/sa110/arm/ \
					 -e s/s390x/s390/ -e s/parisc64/parisc/ \
					 -e s/ppc.*/powerpc/ -e s/mips.*/mips/ )

ARCH ?= $(SUBARCH)


UTS_MACHINE := $(ARCH)


CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)




KBUILD_MODULES :=
KBUILD_BUILTIN := 1






ifeq ($(MAKECMDGOALS),modules)
  KBUILD_BUILTIN := $(if $(CONFIG_MODVERSIONS),1)
endif





ifneq ($(filter all _all modules,$(MAKECMDGOALS)),)
  KBUILD_MODULES := 1
endif

ifeq ($(MAKECMDGOALS),)
  KBUILD_MODULES := 1
endif

export KBUILD_MODULES KBUILD_BUILTIN
export KBUILD_CHECKSRC KBUILD_SRC KBUILD_EXTMOD
























ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif




ifneq ($(findstring s,$(MAKEFLAGS)),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE



MAKEFLAGS += --include-dir=$(srctree)

HOSTCC  	= gcc
HOSTCXX  	= g++
HOSTCFLAGS	:=
HOSTCXXFLAGS	:=

include $(srctree)/scripts/Kbuild.include

HOSTCFLAGS	+= $(call hostcc-option,-Wall -Wstrict-prototypes -O2 -fomit-frame-pointer,)
HOSTCXXFLAGS	+= -O2




MAKEFLAGS += -rR



AS		= $(CROSS_COMPILE)as
CC		= $(CROSS_COMPILE)gcc
LD		= $(CC) -nostdlib
CPP		= $(CC) -E
AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump
PKG_CONFIG	?= $(CROSS_COMPILE)pkg-config
AWK		= awk
GENKSYMS	= scripts/genksyms/genksyms
DEPMOD		= /sbin/depmod
KALLSYMS	= scripts/kallsyms
PERL		= perl
CHECK		= sparse

CHECKFLAGS     := -D__linux__ -Dlinux -D__STDC__ -Dunix -D__unix__ -Wbitwise $(CF)
MODFLAGS	= -DMODULE
CFLAGS_MODULE   = $(MODFLAGS)
AFLAGS_MODULE   = $(MODFLAGS)
LDFLAGS_MODULE  = -r
CFLAGS_KERNEL	=
AFLAGS_KERNEL	=




CFLAGS		:= $(CFLAGS)

CFLAGS_busybox	:= $(CFLAGS_busybox)
CPPFLAGS	:= $(CPPFLAGS)
AFLAGS		:= $(AFLAGS)
LDFLAGS		:= $(LDFLAGS)
LDLIBS		:=


KERNELRELEASE = $(shell cat .kernelrelease 2> /dev/null)
KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

export	VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION \
	ARCH CONFIG_SHELL HOSTCC HOSTCFLAGS CROSS_COMPILE AS LD CC \
	CPP AR NM STRIP OBJCOPY OBJDUMP MAKE AWK GENKSYMS PERL UTS_MACHINE \
	HOSTCXX HOSTCXXFLAGS LDFLAGS_MODULE CHECK CHECKFLAGS

export CPPFLAGS NOSTDINC_FLAGS LINUXINCLUDE OBJCOPYFLAGS LDFLAGS
export CFLAGS CFLAGS_KERNEL CFLAGS_MODULE
export AFLAGS AFLAGS_KERNEL AFLAGS_MODULE
export FLTFLAGS




export MODVERDIR := $(if $(KBUILD_EXTMOD),$(firstword $(KBUILD_EXTMOD))/).tmp_versions



RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS -o -name .pc -o -name .hg -o -name .git \) -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn --exclude CVS --exclude .pc --exclude .hg --exclude .git





PHONY += scripts_basic
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic


scripts/basic/%: scripts_basic ;


PHONY += gen_build_files
gen_build_files: $(wildcard $(srctree)/*/*.c) $(wildcard $(srctree)/*/*/*.c) $(wildcard $(srctree)/embed/*)
	$(Q)$(srctree)/scripts/gen_build_files.sh $(srctree) $(objtree)




PHONY += applets_dir
applets_dir: scripts_basic gen_build_files include/config/MARKER
	$(Q)$(MAKE) $(build)=applets

applets/%: applets_dir ;

PHONY += outputmakefile



outputmakefile:
ifneq ($(KBUILD_SRC),)
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkmakefile \
	    $(srctree) $(objtree) $(VERSION) $(PATCHLEVEL)
endif









no-dot-config-targets := clean mrproper distclean \
			 cscope TAGS tags help %docs


config-targets := 0
mixed-targets  := 0
dot-config     := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter config %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(filter-out config %config,$(MAKECMDGOALS)),)
                        mixed-targets := 1
                endif
        endif
endif

ifeq ($(mixed-targets),1)




%:: FORCE
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= $@

else
ifeq ($(config-targets),1)







-include $(srctree)/arch/$(ARCH)/Makefile
export KBUILD_DEFCONFIG

config: scripts_basic outputmakefile gen_build_files FORCE
	$(Q)mkdir -p include
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= .kernelrelease

%config: scripts_basic outputmakefile gen_build_files FORCE
	$(Q)mkdir -p include
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= .kernelrelease

else




ifeq ($(KBUILD_EXTMOD),)



PHONY += scripts
scripts: gen_build_files scripts_basic include/config/MARKER
	$(Q)$(MAKE) $(build)=$(@)

scripts_basic: include/autoconf.h


core-y		:= \
		applets/ \

libs-y		:= \
		archival/ \
		archival/libarchive/ \
		console-tools/ \
		coreutils/ \
		coreutils/libcoreutils/ \
		debianutils/ \
		klibc-utils/ \
		e2fsprogs/ \
		editors/ \
		findutils/ \
		init/ \
		libbb/ \
		libpwdgrp/ \
		loginutils/ \
		mailutils/ \
		miscutils/ \
		modutils/ \
		networking/ \
		networking/libiproute/ \
		networking/udhcp/ \
		printutils/ \
		procps/ \
		runit/ \
		selinux/ \
		shell/ \
		sysklogd/ \
		util-linux/ \
		util-linux/volume_id/ \

endif 

ifeq ($(dot-config),1)




-include .kconfig.d

-include .config




.config .kconfig.d: ;

-include $(srctree)/arch/$(ARCH)/Makefile


include $(srctree)/Makefile.flags





include/autoconf.h: .kconfig.d .config $(wildcard $(srctree)/*/*.c) $(wildcard $(srctree)/*/*/*.c) | gen_build_files
	$(Q)$(MAKE) -f $(srctree)/Makefile silentoldconfig

include/usage.h: gen_build_files

else

include/autoconf.h: ;
endif





all: busybox doc



CHECKFLAGS += $(NOSTDINC_FLAGS)






export KBUILD_IMAGE ?= busybox




export	INSTALL_PATH ?= /boot







MODLIB	= $(INSTALL_MOD_PATH)/lib/modules/$(KERNELRELEASE)
export MODLIB


ifeq ($(KBUILD_EXTMOD),)
busybox-dirs	:= $(patsubst %/,%,$(filter %/, $(core-y) $(core-m) $(libs-y) $(libs-m)))

busybox-alldirs	:= $(sort $(busybox-dirs) $(patsubst %/,%,$(filter %/, \
		     $(core-n) $(core-) $(libs-n) $(libs-) \
		)))

core-y		:= $(patsubst %/, %/built-in.o, $(core-y))
libs-y1		:= $(patsubst %/, %/lib.a, $(libs-y))
libs-y2		:= $(patsubst %/, %/built-in.o, $(libs-y))
libs-y		:= $(libs-y1) $(libs-y2)




























busybox-all  := $(core-y) $(libs-y)



quiet_cmd_busybox__ ?= LINK    $@
      cmd_busybox__ ?= $(srctree)/scripts/trylink \
      "$@" \
      "$(CC)" \
      "$(CFLAGS) $(CFLAGS_busybox)" \
      "$(LDFLAGS) $(EXTRA_LDFLAGS)" \
      "$(core-y)" \
      "$(libs-y)" \
      "$(LDLIBS)" \
      "$(CONFIG_EXTRA_LDLIBS)" \
      && $(srctree)/scripts/generate_BUFSIZ.sh --post include/common_bufsiz.h


quiet_cmd_sysmap = SYSMAP
      cmd_sysmap = $(CONFIG_SHELL) $(srctree)/scripts/mksysmap






define rule_busybox__
	:
	$(call cmd,busybox__)
	$(Q)echo 'cmd_$@ := $(cmd_busybox__)' > $(@D)/.$(@F).cmd
endef


ifdef CONFIG_KALLSYMS

















ifdef CONFIG_KALLSYMS_EXTRA_PASS
last_kallsyms := 3
else
last_kallsyms := 2
endif

kallsyms.o := .tmp_kallsyms$(last_kallsyms).o

define verify_kallsyms
	$(Q)$(if $($(quiet)cmd_sysmap),                       \
	  echo '  $($(quiet)cmd_sysmap) .tmp_System.map' &&)  \
	  $(cmd_sysmap) .tmp_busybox$(last_kallsyms) .tmp_System.map
	$(Q)cmp -s System.map .tmp_System.map ||              \
		(echo Inconsistent kallsyms data;             \
		 echo Try setting CONFIG_KALLSYMS_EXTRA_PASS; \
		 rm .tmp_kallsyms* ; /bin/false )
endef




cmd_ksym_ld = $(cmd_busybox__)
define rule_ksym_ld
	:
	+$(call cmd,busybox_version)
	$(call cmd,busybox__)
	$(Q)echo 'cmd_$@ := $(cmd_busybox__)' > $(@D)/.$(@F).cmd
endef


quiet_cmd_kallsyms = KSYM    $@
      cmd_kallsyms = $(NM) -n $< | $(KALLSYMS) \
                     $(if $(CONFIG_KALLSYMS_ALL),--all-symbols) > $@

.tmp_kallsyms1.o .tmp_kallsyms2.o .tmp_kallsyms3.o: %.o: %.S scripts FORCE
	$(call if_changed_dep,as_o_S)

.tmp_kallsyms%.S: .tmp_busybox% $(KALLSYMS)
	$(call cmd,kallsyms)


.tmp_busybox1: $(busybox-lds) $(busybox-all) FORCE
	$(call if_changed_rule,ksym_ld)

.tmp_busybox2: $(busybox-lds) $(busybox-all) .tmp_kallsyms1.o FORCE
	$(call if_changed,busybox__)

.tmp_busybox3: $(busybox-lds) $(busybox-all) .tmp_kallsyms2.o FORCE
	$(call if_changed,busybox__)


$(KALLSYMS): scripts ;


debug_kallsyms: .tmp_map$(last_kallsyms)

.tmp_map%: .tmp_busybox% FORCE
	($(OBJDUMP) -h $< | $(AWK) '/^ +[0-9]/{print $$4 " 0 " $$2}'; $(NM) $<) | sort > $@

.tmp_map3: .tmp_map2

.tmp_map2: .tmp_map1

endif 


busybox_unstripped: $(busybox-all) FORCE
	$(call if_changed_rule,busybox__)
	$(Q)rm -f .old_version

busybox: busybox_unstripped
ifeq ($(SKIP_STRIP),y)
	$(Q)cp $< $@
else
	$(Q)$(STRIP) -s --remove-section=.note --remove-section=.comment \
		busybox_unstripped -o $@

	$(Q)chmod a+x $@
endif



$(sort $(busybox-all)): $(busybox-dirs) ;







PHONY += $(busybox-dirs)
$(busybox-dirs): prepare scripts
	$(Q)$(MAKE) $(build)=$@









nullstring :=
space      := $(nullstring) 

___localver = $(objtree)/localversion* $(srctree)/localversion*
__localver  = $(sort $(wildcard $(___localver)))

_localver = $(foreach f, $(__localver), $(if $(findstring ~, $(f)),,$(f)))

localver = $(subst $(space),, \
	   $(shell cat /dev/null $(_localver)) \
	   $(patsubst "%",%,$(CONFIG_LOCALVERSION)))








ifdef _BB_DISABLED_CONFIG_LOCALVERSION_AUTO
	_localver-auto = $(shell $(CONFIG_SHELL) \
	                  $(srctree)/scripts/setlocalversion $(srctree))
	localver-auto  = $(LOCALVERSION)$(_localver-auto)
endif

localver-full = $(localver)$(localver-auto)


kernelrelease = $(KERNELVERSION)$(localver-full)
.kernelrelease: FORCE
	$(Q)rm -f $@
	$(Q)echo $(kernelrelease) > $@









PHONY += prepare archprepare prepare0 prepare1 prepare2 prepare3


PHONY += prepare-all





prepare3: .kernelrelease
ifneq ($(KBUILD_SRC),)
	@echo '  Using $(srctree) as source for busybox'
	$(Q)if [ -f $(srctree)/.config ]; then \
		echo "  $(srctree) is not clean, please run 'make mrproper'";\
		echo "  in the '$(srctree)' directory.";\
		/bin/false; \
	fi;
	$(Q)if [ ! -d include2 ]; then mkdir -p include2; fi;
	$(Q)ln -fsn $(srctree)/include/asm-$(ARCH) include2/asm
endif


prepare2: prepare3 outputmakefile

prepare1: prepare2 include/config/MARKER
ifneq ($(KBUILD_MODULES),)
	$(Q)mkdir -p $(MODVERDIR)
	$(Q)rm -f $(MODVERDIR)/*
endif

archprepare: prepare1 scripts_basic applets_dir

prepare0: archprepare FORCE
	$(Q)$(MAKE) $(build)=.


prepare prepare-all: prepare0




export CPPFLAGS_busybox.lds += -P -C -U$(ARCH)











quiet_cmd_gen_bbconfigopts = GEN     include/bbconfigopts.h
      cmd_gen_bbconfigopts = $(srctree)/scripts/mkconfigs include/bbconfigopts.h include/bbconfigopts_bz2.h
quiet_cmd_gen_common_bufsiz = GEN     include/common_bufsiz.h
      cmd_gen_common_bufsiz = $(srctree)/scripts/generate_BUFSIZ.sh include/common_bufsiz.h
quiet_cmd_split_autoconf   = SPLIT   include/autoconf.h -> include/config/*
      cmd_split_autoconf   = scripts/basic/split-include include/autoconf.h include/config
quiet_cmd_gen_embedded_scripts = GEN     include/embedded_scripts.h
      cmd_gen_embedded_scripts = $(srctree)/scripts/embedded_scripts include/embedded_scripts.h $(srctree)/embed $(srctree)/applets_sh

include/config/MARKER: scripts/basic/split-include include/autoconf.h $(wildcard $(srctree)/embed/*) $(wildcard $(srctree)/applets_sh/*) $(srctree)/scripts/embedded_scripts
	$(call cmd,split_autoconf)
	$(call cmd,gen_bbconfigopts)
	$(call cmd,gen_common_bufsiz)
	$(call cmd,gen_embedded_scripts)
	@touch $@







uts_len := 64

define filechk_version.h
	if [ `echo -n "$(KERNELRELEASE)" | wc -c ` -gt $(uts_len) ]; then \
	  echo '"$(KERNELRELEASE)" exceeds $(uts_len) characters' >&2; \
	  exit 1; \
	fi; \
	(echo \
	  echo \
	 echo '
	)
endef



PHONY += depend dep
depend dep:
	@echo '*** Warning: make $@ is unnecessary now.'




ifdef _BB_DISABLED_CONFIG_MODULES



all: modules



PHONY += modules
modules: $(busybox-dirs) $(if $(KBUILD_BUILTIN),busybox)
	@echo '  Building modules, stage 2.';
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modpost



PHONY += modules_prepare
modules_prepare: prepare scripts


PHONY += modules_install
modules_install: _modinst_ _modinst_post

PHONY += _modinst_
_modinst_:
	@if [ -z "`$(DEPMOD) -V 2>/dev/null | grep module-init-tools`" ]; then \
		echo "Warning: you may need to install module-init-tools"; \
		echo "See http://www.codemonkey.org.uk/docs/post-halloween-2.6.txt";\
		sleep 1; \
	fi
	@rm -rf $(MODLIB)/kernel
	@rm -f $(MODLIB)/source
	@mkdir -p $(MODLIB)/kernel
	@ln -s $(srctree) $(MODLIB)/source
	@if [ ! $(objtree) -ef  $(MODLIB)/build ]; then \
		rm -f $(MODLIB)/build ; \
		ln -s $(objtree) $(MODLIB)/build ; \
	fi
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modinst






ifeq "$(strip $(INSTALL_MOD_PATH))" ""
depmod_opts	:=
else
depmod_opts	:= -b $(INSTALL_MOD_PATH) -r
endif
PHONY += _modinst_post
_modinst_post: _modinst_
	if [ -r System.map -a -x $(DEPMOD) ]; then $(DEPMOD) -ae -F System.map $(depmod_opts) $(KERNELRELEASE); fi

else 




modules modules_install: FORCE
	@echo
	@echo "The present busybox configuration has modules disabled."
	@echo "Type 'make config' and enable loadable module support."
	@echo "Then build a kernel with module support enabled."
	@echo
	@exit 1

endif 









CLEAN_DIRS  += $(MODVERDIR) _install 0_lib
CLEAN_FILES +=	busybox busybox_unstripped* busybox.links \
                System.map .kernelrelease \
                .tmp_kallsyms* .tmp_version .tmp_busybox* .tmp_System.map


MRPROPER_DIRS  += include/config include2
MRPROPER_FILES += .config .config.old include/asm .version .old_version \
		  include/NUM_APPLETS.h \
		  include/common_bufsiz.h \
		  include/autoconf.h \
		  include/bbconfigopts.h \
		  include/bbconfigopts_bz2.h \
		  include/embedded_scripts.h \
		  include/usage_compressed.h \
		  include/applet_tables.h \
		  include/applets.h \
		  include/usage.h \
		  applets/usage \
		  .kernelrelease Module.symvers tags TAGS cscope* \
		  busybox_old



clean: rm-dirs  := $(CLEAN_DIRS)
clean: rm-files := $(CLEAN_FILES)
clean-dirs      := $(addprefix _clean_,$(srctree) $(busybox-alldirs))

PHONY += $(clean-dirs) clean archclean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

clean: archclean $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find . $(RCS_FIND_IGNORE) \
		\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \) \
		-type f -print | xargs rm -f

PHONY += doc-clean
doc-clean: rm-files := docs/busybox.pod \
		  docs/BusyBox.html docs/busybox.1 docs/BusyBox.txt
doc-clean:
	$(call cmd,rmfiles)



mrproper: rm-dirs  := $(wildcard $(MRPROPER_DIRS))
mrproper: rm-files := $(wildcard $(MRPROPER_FILES))
mrproper-dirs      := $(addprefix _mrproper_,scripts)

PHONY += $(mrproper-dirs) mrproper archmrproper
$(mrproper-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _mrproper_%,%,$@)

mrproper: clean archmrproper $(mrproper-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find . -name Config.src | sed 's/.src$$/.in/' | xargs -r rm -f
	@find . -name Kbuild.src | sed 's/.src$$//' | xargs -r rm -f



PHONY += distclean

distclean: mrproper
	@find $(srctree) $(RCS_FIND_IGNORE) \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '
		-o -name '.*.rej' -o -name '*.tmp' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -f





package-dir	:= $(srctree)/scripts/package

%pkg: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@
rpm: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@





boards := $(wildcard $(srctree)/configs/*_defconfig)
boards := $(notdir $(boards))

-include $(srctree)/Makefile.help



%docs: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=Documentation/DocBook $@

else 



















KBUILD_MODULES := 1
PHONY += crmodverdir
crmodverdir:
	$(Q)mkdir -p $(MODVERDIR)
	$(Q)rm -f $(MODVERDIR)/*

PHONY += $(objtree)/Module.symvers
$(objtree)/Module.symvers:
	@test -e $(objtree)/Module.symvers || ( \
	echo; \
	echo "  WARNING: Symbol version dump $(objtree)/Module.symvers"; \
	echo "           is missing; modules will have no dependencies and modversions."; \
	echo )

module-dirs := $(addprefix _module_,$(KBUILD_EXTMOD))
PHONY += $(module-dirs) modules
$(module-dirs): crmodverdir $(objtree)/Module.symvers
	$(Q)$(MAKE) $(build)=$(patsubst _module_%,%,$@)

modules: $(module-dirs)
	@echo '  Building modules, stage 2.';
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modpost

PHONY += modules_install
modules_install: _emodinst_ _emodinst_post

install-dir := $(if $(INSTALL_MOD_DIR),$(INSTALL_MOD_DIR),extra)
PHONY += _emodinst_
_emodinst_:
	$(Q)mkdir -p $(MODLIB)/$(install-dir)
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modinst


quiet_cmd_depmod = DEPMOD  $(KERNELRELEASE)
      cmd_depmod = if [ -r System.map -a -x $(DEPMOD) ]; then \
                      $(DEPMOD) -ae -F System.map             \
                      $(if $(strip $(INSTALL_MOD_PATH)),      \
		      -b $(INSTALL_MOD_PATH) -r)              \
		      $(KERNELRELEASE);                       \
                   fi

PHONY += _emodinst_post
_emodinst_post: _emodinst_
	$(call cmd,depmod)

clean-dirs := $(addprefix _clean_,$(KBUILD_EXTMOD))

PHONY += $(clean-dirs) clean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)

clean:	rm-dirs := $(MODVERDIR)
clean: $(clean-dirs)
	$(call cmd,rmdirs)
	@find $(KBUILD_EXTMOD) $(RCS_FIND_IGNORE) \
		\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \) \
		-type f -print | xargs rm -f


PHONY += prepare scripts
prepare: ;
scripts: ;
endif 








ifeq ($(src),$(obj))
__srctree =
else
__srctree = $(srctree)/
endif

ifeq ($(ALLSOURCE_ARCHS),)
ifeq ($(ARCH),um)
ALLINCLUDE_ARCHS := $(ARCH) $(SUBARCH)
else
ALLINCLUDE_ARCHS := $(ARCH)
endif
else

ALLINCLUDE_ARCHS := $(ALLSOURCE_ARCHS)
endif

ALLSOURCE_ARCHS := $(ARCH)

define all-sources
	( find -regex '.*\.[ch]$$' )
endef

quiet_cmd_cscope-file = FILELST cscope.files
      cmd_cscope-file = (echo \-k; echo \-q; $(all-sources)) > cscope.files

quiet_cmd_cscope = MAKE    cscope.out
      cmd_cscope = cscope -b

cscope: FORCE
	$(call cmd,cscope-file)
	$(call cmd,cscope)

quiet_cmd_TAGS = MAKE   $@
define cmd_TAGS
	rm -f $@; \
	ETAGSF=`etags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs etags $$ETAGSF -a
endef

TAGS: FORCE
	$(call cmd,TAGS)


quiet_cmd_tags = MAKE   $@
define cmd_tags
	rm -f $@; \
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs ctags $$CTAGSF -a
endef

tags: FORCE
	$(call cmd,tags)





includecheck:
	find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort \
		| xargs $(PERL) -w scripts/checkincludes.pl

versioncheck:
	find * $(RCS_FIND_IGNORE) \
		-name '*.[hcS]' -type f -print | sort \
		| xargs $(PERL) -w scripts/checkversion.pl

namespacecheck:
	$(PERL) $(srctree)/scripts/namespace.pl

endif 
endif 

PHONY += checkstack
checkstack:
	$(OBJDUMP) -d busybox $$(find . -name '*.ko') | \
	$(PERL) $(src)/scripts/checkstack.pl $(ARCH)

kernelrelease:
	$(if $(wildcard .kernelrelease), $(Q)echo $(KERNELRELEASE), \
	$(error kernelrelease not valid - run 'make *config' to update it))
kernelversion:
	@echo $(KERNELVERSION)











ifeq ($(KBUILD_EXTMOD),)
        build-dir  = $(patsubst %/,%,$(dir $@))
        target-dir = $(dir $@)
else
        zap-slash=$(filter-out .,$(patsubst %/,%,$(dir $@)))
        build-dir  = $(KBUILD_EXTMOD)$(if $(zap-slash),/$(zap-slash))
        target-dir = $(if $(KBUILD_EXTMOD),$(dir $<),$(dir $@))
endif

%.s: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.i: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.lst: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.s: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)


%/: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1) \
	$(build)=$(build-dir)
/: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1) \
	$(build)=$(build-dir)

%.ko: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(CONFIG_MODULES),1)   \
	$(build)=$(build-dir) $(@:.ko=.o)
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modpost




quiet_cmd_rmdirs = $(if $(wildcard $(rm-dirs)),CLEAN   $(wildcard $(rm-dirs)))
      cmd_rmdirs = rm -rf $(rm-dirs)

quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),CLEAN   $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)



targets := $(wildcard $(sort $(targets)))
cmd_files := $(wildcard .*.cmd $(foreach f,$(targets),$(dir $(f)).$(notdir $(f)).cmd))

ifneq ($(cmd_files),)
  $(cmd_files): ;	
  include $(cmd_files)
endif




clean := -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.clean obj

endif	

PHONY += FORCE
FORCE:

-include $(srctree)/Makefile.custom



.PHONY: $(PHONY)
