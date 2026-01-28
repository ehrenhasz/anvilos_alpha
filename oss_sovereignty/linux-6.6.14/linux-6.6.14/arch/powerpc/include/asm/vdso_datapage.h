#ifndef _VDSO_DATAPAGE_H
#define _VDSO_DATAPAGE_H
#ifdef __KERNEL__
#define SYSTEMCFG_MAJOR 1
#define SYSTEMCFG_MINOR 1
#ifndef __ASSEMBLY__
#include <linux/unistd.h>
#include <linux/time.h>
#include <vdso/datapage.h>
#define SYSCALL_MAP_SIZE      ((NR_syscalls + 31) / 32)
#ifdef CONFIG_PPC64
struct vdso_arch_data {
	__u8  eye_catcher[16];		 
	struct {			 
		__u32 major;		 
		__u32 minor;		 
	} version;
	__u32 platform;			 
	__u32 processor;		 
	__u64 processorCount;		 
	__u64 physicalMemorySize;	 
	__u64 tb_orig_stamp;		 
	__u64 tb_ticks_per_sec;		 
	__u64 tb_to_xs;			 
	__u64 stamp_xsec;		 
	__u64 tb_update_count;		 
	__u32 tz_minuteswest;		 
	__u32 tz_dsttime;		 
	__u32 dcache_size;		 
	__u32 dcache_line_size;		 
	__u32 icache_size;		 
	__u32 icache_line_size;		 
	__u32 dcache_block_size;		 
	__u32 icache_block_size;		 
	__u32 dcache_log_block_size;		 
	__u32 icache_log_block_size;		 
	__u32 syscall_map[SYSCALL_MAP_SIZE];	 
	__u32 compat_syscall_map[SYSCALL_MAP_SIZE];	 
	struct vdso_data data[CS_BASES];
};
#else  
struct vdso_arch_data {
	__u64 tb_ticks_per_sec;		 
	__u32 syscall_map[SYSCALL_MAP_SIZE];  
	__u32 compat_syscall_map[0];	 
	struct vdso_data data[CS_BASES];
};
#endif  
extern struct vdso_arch_data *vdso_data;
#else  
.macro get_datapage ptr
	bcl	20, 31, .+4
999:
	mflr	\ptr
	addis	\ptr, \ptr, (_vdso_datapage - 999b)@ha
	addi	\ptr, \ptr, (_vdso_datapage - 999b)@l
.endm
#endif  
#endif  
#endif  
