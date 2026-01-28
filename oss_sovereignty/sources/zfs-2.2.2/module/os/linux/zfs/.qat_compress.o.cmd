savedcmd_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o := gcc -Wp,-MMD,/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/.qat_compress.o.d -nostdinc -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/compiler-version.h -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=./= -Werror -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -fcf-protection=branch -fno-jump-tables -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -mindirect-branch-cs-prefix -mfunction-return=thunk-extern -fno-jump-tables -fpatchable-function-entry=16,16 -fno-delete-null-pointer-checks -O2 -fno-allow-store-data-races -fstack-protector-strong -fomit-frame-pointer -ftrivial-auto-var-init=zero -fno-stack-clash-protection -falign-functions=16 -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fconserve-stack -Wall -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wframe-larger-than=2048 -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-dangling-pointer -Wvla -Wno-pointer-sign -Wcast-function-type -Wno-array-bounds -Wno-alloc-size-larger-than -Wimplicit-fallthrough=5 -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wenum-conversion -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-restrict -Wno-packed-not-aligned -Wno-format-overflow -Wno-format-truncation -Wno-stringop-overflow -Wno-stringop-truncation -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-maybe-uninitialized -Wno-sign-compare -std=gnu99 -Wno-declaration-after-statement -Wmissing-prototypes -Wno-format-zero-length -include /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/zfs_config.h -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/kernel -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/spl -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/zfs -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -D_KERNEL -UDEBUG -DNDEBUG  -DMODULE  -DKBUILD_BASENAME='"qat_compress"' -DKBUILD_MODNAME='"zfs"' -D__KBUILD_MODNAME=kmod_zfs -c -o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.c  

source_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o := /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.c

deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o := \
  include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/CC_IS_GCC) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  include/linux/compiler_attributes.h \
  include/linux/compiler-gcc.h \
    $(wildcard include/config/RETPOLINE) \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/SHADOW_CALL_STACK) \
    $(wildcard include/config/KCOV) \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/zfs_config.h \

/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o: $(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o)

$(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/os/linux/zfs/qat_compress.o):
