





include $(srctree)/lib/vdso/Makefile


KASAN_SANITIZE			:= n
KMSAN_SANITIZE_vclock_gettime.o := n
KMSAN_SANITIZE_vgetcpu.o	:= n

UBSAN_SANITIZE			:= n
KCSAN_SANITIZE			:= n
OBJECT_FILES_NON_STANDARD	:= y


KCOV_INSTRUMENT		:= n

VDSO64-$(CONFIG_X86_64)		:= y
VDSOX32-$(CONFIG_X86_X32_ABI)	:= y
VDSO32-$(CONFIG_X86_32)		:= y
VDSO32-$(CONFIG_IA32_EMULATION)	:= y


vobjs-y := vdso-note.o vclock_gettime.o vgetcpu.o
vobjs32-y := vdso32/note.o vdso32/system_call.o vdso32/sigreturn.o
vobjs32-y += vdso32/vclock_gettime.o vdso32/vgetcpu.o
vobjs-$(CONFIG_X86_SGX)	+= vsgx.o


obj-y					+= vma.o extable.o
KASAN_SANITIZE_vma.o			:= y
UBSAN_SANITIZE_vma.o			:= y
KCSAN_SANITIZE_vma.o			:= y
OBJECT_FILES_NON_STANDARD_vma.o		:= n
OBJECT_FILES_NON_STANDARD_extable.o	:= n


vdso_img-$(VDSO64-y)		+= 64
vdso_img-$(VDSOX32-y)		+= x32
vdso_img-$(VDSO32-y)		+= 32

obj-$(VDSO32-y)			+= vdso32-setup.o

vobjs := $(foreach F,$(vobjs-y),$(obj)/$F)
vobjs32 := $(foreach F,$(vobjs32-y),$(obj)/$F)

$(obj)/vdso.o: $(obj)/vdso.so

targets += vdso.lds $(vobjs-y)
targets += vdso32/vdso32.lds $(vobjs32-y)


vdso_img_objs := $(vdso_img-y:%=vdso-image-%.o)
vdso_img_cfiles := $(vdso_img-y:%=vdso-image-%.c)
vdso_img_sodbg := $(vdso_img-y:%=vdso%.so.dbg)
obj-y += $(vdso_img_objs)
targets += $(vdso_img_cfiles)
targets += $(vdso_img_sodbg) $(vdso_img-y:%=vdso%.so)

CPPFLAGS_vdso.lds += -P -C

VDSO_LDFLAGS_vdso.lds = -m elf_x86_64 -soname linux-vdso.so.1 --no-undefined \
			-z max-page-size=4096

$(obj)/vdso64.so.dbg: $(obj)/vdso.lds $(vobjs) FORCE
	$(call if_changed,vdso_and_check)

HOST_EXTRACFLAGS += -I$(srctree)/tools/include -I$(srctree)/include/uapi -I$(srctree)/arch/$(SUBARCH)/include/uapi
hostprogs += vdso2c

quiet_cmd_vdso2c = VDSO2C  $@
      cmd_vdso2c = $(obj)/vdso2c $< $(<:%.dbg=%) $@

$(obj)/vdso-image-%.c: $(obj)/vdso%.so.dbg $(obj)/vdso%.so $(obj)/vdso2c FORCE
	$(call if_changed,vdso2c)





CFL := $(PROFILING) -mcmodel=small -fPIC -O2 -fasynchronous-unwind-tables -m64 \
       $(filter -g%,$(KBUILD_CFLAGS)) -fno-stack-protector \
       -fno-omit-frame-pointer -foptimize-sibling-calls \
       -DDISABLE_BRANCH_PROFILING -DBUILD_VDSO

ifdef CONFIG_RETPOLINE
ifneq ($(RETPOLINE_VDSO_CFLAGS),)
  CFL += $(RETPOLINE_VDSO_CFLAGS)
endif
endif

$(vobjs): KBUILD_CFLAGS := $(filter-out $(PADDING_CFLAGS) $(CC_FLAGS_LTO) $(CC_FLAGS_CFI) $(RANDSTRUCT_CFLAGS) $(GCC_PLUGINS_CFLAGS) $(RETPOLINE_CFLAGS),$(KBUILD_CFLAGS)) $(CFL)
$(vobjs): KBUILD_AFLAGS += -DBUILD_VDSO




CFLAGS_REMOVE_vclock_gettime.o = -pg
CFLAGS_REMOVE_vdso32/vclock_gettime.o = -pg
CFLAGS_REMOVE_vgetcpu.o = -pg
CFLAGS_REMOVE_vdso32/vgetcpu.o = -pg
CFLAGS_REMOVE_vsgx.o = -pg











CPPFLAGS_vdsox32.lds = $(CPPFLAGS_vdso.lds)
VDSO_LDFLAGS_vdsox32.lds = -m elf32_x86_64 -soname linux-vdso.so.1 \
			   -z max-page-size=4096


vobjx32s-y := $(vobjs-y:.o=-x32.o)


vobjx32s := $(foreach F,$(vobjx32s-y),$(obj)/$F)


quiet_cmd_x32 = X32     $@
      cmd_x32 = $(OBJCOPY) -O elf32-x86-64 $< $@

$(obj)/%-x32.o: $(obj)/%.o FORCE
	$(call if_changed,x32)

targets += vdsox32.lds $(vobjx32s-y)

$(obj)/%.so: OBJCOPYFLAGS := -S --remove-section __ex_table
$(obj)/%.so: $(obj)/%.so.dbg FORCE
	$(call if_changed,objcopy)

$(obj)/vdsox32.so.dbg: $(obj)/vdsox32.lds $(vobjx32s) FORCE
	$(call if_changed,vdso_and_check)

CPPFLAGS_vdso32/vdso32.lds = $(CPPFLAGS_vdso.lds)
VDSO_LDFLAGS_vdso32.lds = -m elf_i386 -soname linux-gate.so.1

KBUILD_AFLAGS_32 := $(filter-out -m64,$(KBUILD_AFLAGS)) -DBUILD_VDSO
$(obj)/vdso32.so.dbg: KBUILD_AFLAGS = $(KBUILD_AFLAGS_32)
$(obj)/vdso32.so.dbg: asflags-$(CONFIG_X86_64) += -m32

KBUILD_CFLAGS_32 := $(filter-out -m64,$(KBUILD_CFLAGS))
KBUILD_CFLAGS_32 := $(filter-out -mcmodel=kernel,$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out -fno-pic,$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out -mfentry,$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(RANDSTRUCT_CFLAGS),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(GCC_PLUGINS_CFLAGS),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(RETPOLINE_CFLAGS),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(CC_FLAGS_LTO),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(CC_FLAGS_CFI),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 := $(filter-out $(PADDING_CFLAGS),$(KBUILD_CFLAGS_32))
KBUILD_CFLAGS_32 += -m32 -msoft-float -mregparm=0 -fpic
KBUILD_CFLAGS_32 += -fno-stack-protector
KBUILD_CFLAGS_32 += $(call cc-option, -foptimize-sibling-calls)
KBUILD_CFLAGS_32 += -fno-omit-frame-pointer
KBUILD_CFLAGS_32 += -DDISABLE_BRANCH_PROFILING

ifdef CONFIG_RETPOLINE
ifneq ($(RETPOLINE_VDSO_CFLAGS),)
  KBUILD_CFLAGS_32 += $(RETPOLINE_VDSO_CFLAGS)
endif
endif

$(obj)/vdso32.so.dbg: KBUILD_CFLAGS = $(KBUILD_CFLAGS_32)

$(obj)/vdso32.so.dbg: $(obj)/vdso32/vdso32.lds $(vobjs32) FORCE
	$(call if_changed,vdso_and_check)




quiet_cmd_vdso = VDSO    $@
      cmd_vdso = $(LD) -o $@ \
		       $(VDSO_LDFLAGS) $(VDSO_LDFLAGS_$(filter %.lds,$(^F))) \
		       -T $(filter %.lds,$^) $(filter %.o,$^) && \
		 sh $(srctree)/$(src)/checkundef.sh '$(NM)' '$@'

VDSO_LDFLAGS = -shared --hash-style=both --build-id=sha1 \
	$(call ld-option, --eh-frame-hdr) -Bsymbolic -z noexecstack
GCOV_PROFILE := n

quiet_cmd_vdso_and_check = VDSO    $@
      cmd_vdso_and_check = $(cmd_vdso); $(cmd_vdso_check)





quiet_cmd_vdso_install = INSTALL $(@:install_%=%)
define cmd_vdso_install
	cp $< "$(MODLIB)/vdso/$(@:install_%=%)"; \
	if readelf -n $< |grep -q 'Build ID'; then \
	  buildid=`readelf -n $< |grep 'Build ID' |sed -e 's/^.*Build ID: \(.*\)$$/\1/'`; \
	  first=`echo $$buildid | cut -b-2`; \
	  last=`echo $$buildid | cut -b3-`; \
	  mkdir -p "$(MODLIB)/vdso/.build-id/$$first"; \
	  ln -sf "../../$(@:install_%=%)" "$(MODLIB)/vdso/.build-id/$$first/$$last.debug"; \
	fi
endef

vdso_img_insttargets := $(vdso_img_sodbg:%.dbg=install_%)

$(MODLIB)/vdso: FORCE
	@mkdir -p $(MODLIB)/vdso

$(vdso_img_insttargets): install_%: $(obj)/%.dbg $(MODLIB)/vdso
	$(call cmd,vdso_install)

PHONY += vdso_install $(vdso_img_insttargets)
vdso_install: $(vdso_img_insttargets)

clean-files := vdso32.so vdso32.so.dbg vdso64* vdso-image-*.c vdsox32.so*
