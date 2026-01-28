savedcmd_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o := gcc -Wp,-MMD,/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/.ghash-x86_64.o.d -nostdinc -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/compiler-version.h -include ./include/linux/kconfig.h -D__KERNEL__ -fmacro-prefix-map=./= -Werror -D__ASSEMBLY__ -fno-PIE -m64 -std=gnu99 -Wno-declaration-after-statement -Wmissing-prototypes -Wno-format-zero-length -include /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/zfs_config.h -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/kernel -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/spl -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/zfs -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include -D_KERNEL -UDEBUG -DNDEBUG -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/include -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/spl -I/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include  -DMODULE  -c -o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.S 

source_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o := /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.S

deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o := \
  include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/zfs_config.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/sys/asm_linkage.h \
  /home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/include/os/linux/spl/sys/ia32/asm_linkage.h \
  include/linux/linkage.h \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/CC_IS_GCC) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  include/linux/stringify.h \
  include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/64BIT) \
  include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
  arch/x86/include/generated/asm/rwonce.h \
  include/asm-generic/rwonce.h \
  arch/x86/include/asm/linkage.h \
    $(wildcard include/config/X86_32) \
    $(wildcard include/config/CALL_PADDING) \
    $(wildcard include/config/RETHUNK) \
    $(wildcard include/config/RETPOLINE) \
    $(wildcard include/config/SLS) \
    $(wildcard include/config/FUNCTION_PADDING_BYTES) \
    $(wildcard include/config/UML) \
  arch/x86/include/asm/ibt.h \
    $(wildcard include/config/X86_KERNEL_IBT) \
  include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  include/uapi/linux/types.h \
  arch/x86/include/generated/uapi/asm/types.h \
  include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/x86/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  arch/x86/include/asm/frame.h \
    $(wildcard include/config/FRAME_POINTER) \
  arch/x86/include/asm/asm.h \
    $(wildcard include/config/KPROBES) \
  arch/x86/include/asm/extable_fixup_types.h \

/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o: $(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o)

$(deps_/home/aimeat/anvilos/oss_sovereignty/zfs-2.2.2/module/icp/asm-x86_64/modes/ghash-x86_64.o):
