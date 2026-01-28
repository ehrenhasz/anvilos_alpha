#ifndef __SPARC_IPCBUF_H
#define __SPARC_IPCBUF_H
#include <linux/posix_types.h>
struct ipc64_perm
{
	__kernel_key_t		key;
	__kernel_uid32_t	uid;
	__kernel_gid32_t	gid;
	__kernel_uid32_t	cuid;
	__kernel_gid32_t	cgid;
#ifndef __arch64__
	unsigned short		__pad0;
#endif
	__kernel_mode_t		mode;
	unsigned short		__pad1;
	unsigned short		seq;
	unsigned long long	__unused1;
	unsigned long long	__unused2;
};
#endif  
