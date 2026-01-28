#ifndef _ASM_HYPFS_H
#define _ASM_HYPFS_H
#include <linux/types.h>
struct hypfs_diag304 {
	__u32	args[2];
	__u64	data;
	__u64	rc;
} __attribute__((packed));
#define HYPFS_IOCTL_MAGIC 0x10
#define HYPFS_DIAG304 \
	_IOWR(HYPFS_IOCTL_MAGIC, 0x20, struct hypfs_diag304)
struct hypfs_diag0c_hdr {
	__u64	len;		 
	__u16	version;	 
	char	reserved1[6];	 
	char	tod_ext[16];	 
	__u64	count;		 
	char	reserved2[24];	 
};
struct hypfs_diag0c_entry {
	char	date[8];	 
	char	time[8];	 
	__u64	virtcpu;	 
	__u64	totalproc;	 
	__u32	cpu;		 
	__u32	reserved;	 
};
struct hypfs_diag0c_data {
	struct hypfs_diag0c_hdr		hdr;		 
	struct hypfs_diag0c_entry	entry[];	 
};
#endif
