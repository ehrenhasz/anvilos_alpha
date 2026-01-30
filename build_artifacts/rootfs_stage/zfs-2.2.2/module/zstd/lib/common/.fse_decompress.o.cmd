savedcmd_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o := gcc -Wp,-MMD,/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/.fse_decompress.o.d -nostdinc -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/compiler-version.h -include ./include/linux/kconfig.h -include ./include/linux/compiler_types.h -D__KERNEL__ -fmacro-prefix-map=./= -Werror -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -fcf-protection=branch -fno-jump-tables -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -Wno-sign-compare -fno-asynchronous-unwind-tables -mindirect-branch=thunk-extern -mindirect-branch-register -mindirect-branch-cs-prefix -mfunction-return=thunk-extern -fno-jump-tables -fpatchable-function-entry=16,16 -fno-delete-null-pointer-checks -O2 -fno-allow-store-data-races -fstack-protector-strong -fomit-frame-pointer -ftrivial-auto-var-init=zero -fno-stack-clash-protection -falign-functions=16 -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fconserve-stack -Wall -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wframe-larger-than=2048 -Wno-main -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-dangling-pointer -Wvla -Wno-pointer-sign -Wcast-function-type -Wno-array-bounds -Wno-alloc-size-larger-than -Wimplicit-fallthrough=5 -Werror=date-time -Werror=incompatible-pointer-types -Werror=designated-init -Wenum-conversion -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-restrict -Wno-packed-not-aligned -Wno-format-overflow -Wno-format-truncation -Wno-stringop-overflow -Wno-stringop-truncation -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-maybe-uninitialized -Wno-sign-compare -std=gnu99 -Wno-declaration-after-statement -Wmissing-prototypes -Wno-format-zero-length -include /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/zfs_config.h -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/kernel -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/spl -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/zfs -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -D_KERNEL -UDEBUG -DNDEBUG -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include -O3 -fno-tree-vectorize -U__BMI__ -Wframe-larger-than=20480 -include /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/aarch64_compat.h -include /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/zstd_compat_wrapper.h -Wp,-w  -DMODULE  -DKBUILD_BASENAME='"fse_decompress"' -DKBUILD_MODNAME='"zfs"' -D__KBUILD_MODNAME=kmod_zfs -c -o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.c  

source_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o := /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.c

deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o := \
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
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/aarch64_compat.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/zstd_compat_wrapper.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/stdlib.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/string.h \
  include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  arch/x86/include/generated/asm/rwonce.h \
  include/asm-generic/rwonce.h \
  include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  include/uapi/linux/types.h \
  arch/x86/include/generated/uapi/asm/types.h \
  include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/x86/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/uapi/linux/posix_types.h \
  include/linux/stddef.h \
  include/uapi/linux/stddef.h \
  arch/x86/include/asm/posix_types.h \
    $(wildcard include/config/X86_32) \
  arch/x86/include/uapi/asm/posix_types_64.h \
  include/uapi/asm-generic/posix_types.h \
  include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  include/linux/err.h \
  arch/x86/include/generated/uapi/asm/errno.h \
  include/uapi/asm-generic/errno.h \
  include/uapi/asm-generic/errno-base.h \
  include/linux/errno.h \
  include/uapi/linux/errno.h \
  include/linux/overflow.h \
  include/linux/limits.h \
  include/uapi/linux/limits.h \
  include/vdso/limits.h \
  include/linux/const.h \
  include/vdso/const.h \
  include/uapi/linux/const.h \
  include/linux/stdarg.h \
  include/uapi/linux/string.h \
  arch/x86/include/asm/string.h \
  arch/x86/include/asm/string_64.h \
    $(wildcard include/config/KMSAN) \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  include/linux/jump_label.h \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  arch/x86/include/asm/jump_label.h \
    $(wildcard include/config/HAVE_JUMP_LABEL_HACK) \
  arch/x86/include/asm/asm.h \
    $(wildcard include/config/KPROBES) \
  include/linux/stringify.h \
  arch/x86/include/asm/extable_fixup_types.h \
  arch/x86/include/asm/nops.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/bitstream.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/mem.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/stddef.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/include/stdint.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/compiler.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/debug.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/error_private.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/zstd_errors.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse.h \

/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o: $(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o)

$(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/zstd/lib/common/fse_decompress.o):
