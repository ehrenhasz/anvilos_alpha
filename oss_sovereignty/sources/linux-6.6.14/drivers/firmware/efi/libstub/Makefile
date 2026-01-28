








cflags-y			:= $(KBUILD_CFLAGS)

cflags-$(CONFIG_X86_32)		:= -march=i386
cflags-$(CONFIG_X86_64)		:= -mcmodel=small
cflags-$(CONFIG_X86)		+= -m$(BITS) -D__KERNEL__ \
				   -fPIC -fno-strict-aliasing -mno-red-zone \
				   -mno-mmx -mno-sse -fshort-wchar \
				   -Wno-pointer-sign \
				   $(call cc-disable-warning, address-of-packed-member) \
				   $(call cc-disable-warning, gnu) \
				   -fno-asynchronous-unwind-tables \
				   $(CLANG_FLAGS)



cflags-$(CONFIG_ARM64)		+= -fpie $(DISABLE_STACKLEAK_PLUGIN) \
				   -fno-unwind-tables -fno-asynchronous-unwind-tables
cflags-$(CONFIG_ARM)		+= -DEFI_HAVE_STRLEN -DEFI_HAVE_STRNLEN \
				   -DEFI_HAVE_MEMCHR -DEFI_HAVE_STRRCHR \
				   -DEFI_HAVE_STRCMP -fno-builtin -fpic \
				   $(call cc-option,-mno-single-pic-base)
cflags-$(CONFIG_RISCV)		+= -fpic
cflags-$(CONFIG_LOONGARCH)	+= -fpie

cflags-$(CONFIG_EFI_PARAMS_FROM_FDT)	+= -I$(srctree)/scripts/dtc/libfdt

KBUILD_CFLAGS			:= $(subst $(CC_FLAGS_FTRACE),,$(cflags-y)) \
				   -Os -DDISABLE_BRANCH_PROFILING \
				   -include $(srctree)/include/linux/hidden.h \
				   -D__NO_FORTIFY \
				   -ffreestanding \
				   -fno-stack-protector \
				   $(call cc-option,-fno-addrsig) \
				   -D__DISABLE_EXPORTS






KBUILD_CFLAGS := $(filter-out $(RANDSTRUCT_CFLAGS), $(KBUILD_CFLAGS))


KBUILD_CFLAGS := $(filter-out $(CC_FLAGS_SCS), $(KBUILD_CFLAGS))

KBUILD_CFLAGS := $(filter-out $(CC_FLAGS_CFI), $(KBUILD_CFLAGS))

KBUILD_CFLAGS := $(filter-out $(CC_FLAGS_LTO), $(KBUILD_CFLAGS))

GCOV_PROFILE			:= n

KASAN_SANITIZE			:= n
KCSAN_SANITIZE			:= n
KMSAN_SANITIZE			:= n
UBSAN_SANITIZE			:= n
OBJECT_FILES_NON_STANDARD	:= y


KCOV_INSTRUMENT			:= n

lib-y				:= efi-stub-helper.o gop.o secureboot.o tpm.o \
				   file.o mem.o random.o randomalloc.o pci.o \
				   skip_spaces.o lib-cmdline.o lib-ctype.o \
				   alignedmem.o relocate.o printk.o vsprintf.o


libfdt-deps			:= fdt_rw.c fdt_ro.c fdt_wip.c fdt.c \
				   fdt_empty_tree.c fdt_sw.c

lib-$(CONFIG_EFI_PARAMS_FROM_FDT) += fdt.o \
				     $(patsubst %.c,lib-%.o,$(libfdt-deps))

$(obj)/lib-%.o: $(srctree)/lib/%.c FORCE
	$(call if_changed_rule,cc_o_c)

lib-$(CONFIG_EFI_GENERIC_STUB)	+= efi-stub.o string.o intrinsics.o systable.o \
				   screen_info.o efi-stub-entry.o

lib-$(CONFIG_ARM)		+= arm32-stub.o
lib-$(CONFIG_ARM64)		+= kaslr.o arm64.o arm64-stub.o smbios.o
lib-$(CONFIG_X86)		+= x86-stub.o
lib-$(CONFIG_X86_64)		+= x86-5lvl.o
lib-$(CONFIG_RISCV)		+= kaslr.o riscv.o riscv-stub.o
lib-$(CONFIG_LOONGARCH)		+= loongarch.o loongarch-stub.o

CFLAGS_arm32-stub.o		:= -DTEXT_OFFSET=$(TEXT_OFFSET)

zboot-obj-$(CONFIG_RISCV)	:= lib-clz_ctz.o lib-ashldi3.o
lib-$(CONFIG_EFI_ZBOOT)		+= zboot.o $(zboot-obj-y)

lib-$(CONFIG_UNACCEPTED_MEMORY) += unaccepted_memory.o bitmap.o find.o

extra-y				:= $(lib-y)
lib-y				:= $(patsubst %.o,%.stub.o,$(lib-y))





STUBCOPY_FLAGS-y		+= --remove-section=.note.gnu.property







STUBCOPY_FLAGS-$(CONFIG_X86)	+= --rename-section .bss=.bss.efistub,load,alloc
STUBCOPY_RELOC-$(CONFIG_X86_32)	:= R_386_32
STUBCOPY_RELOC-$(CONFIG_X86_64)	:= R_X86_64_64






STUBCOPY_FLAGS-$(CONFIG_ARM)	+= --rename-section .data=.data.efistub	\
				   --rename-section .bss=.bss.efistub,load,alloc
STUBCOPY_RELOC-$(CONFIG_ARM)	:= R_ARM_ABS















STUBCOPY_FLAGS-$(CONFIG_ARM64)	+= --prefix-alloc-sections=.init \
				   --prefix-symbols=__efistub_
STUBCOPY_RELOC-$(CONFIG_ARM64)	:= R_AARCH64_ABS




STUBCOPY_FLAGS-$(CONFIG_RISCV)	+= --prefix-alloc-sections=.init \
				   --prefix-symbols=__efistub_
STUBCOPY_RELOC-$(CONFIG_RISCV)	:= R_RISCV_HI20



STUBCOPY_FLAGS-$(CONFIG_LOONGARCH)	+= --prefix-alloc-sections=.init \
					   --prefix-symbols=__efistub_
STUBCOPY_RELOC-$(CONFIG_LOONGARCH)	:= R_LARCH_MARK_LA

$(obj)/%.stub.o: $(obj)/%.o FORCE
	$(call if_changed,stubcopy)







quiet_cmd_stubcopy = STUBCPY $@
      cmd_stubcopy =							\
	$(STRIP) --strip-debug -o $@ $<;				\
	if $(OBJDUMP) -r $@ | grep $(STUBCOPY_RELOC-y); then		\
		echo "$@: absolute symbol references not allowed in the EFI stub" >&2; \
		/bin/false;						\
	fi;								\
	$(OBJCOPY) $(STUBCOPY_FLAGS-y) $< $@
