#ifndef _ASM_SHMBUF_H
#define _ASM_SHMBUF_H
#include <asm/ipcbuf.h>
#include <asm/posix_types.h>
#ifdef __mips64
struct shmid64_ds {
	struct ipc64_perm	shm_perm;	 
	__kernel_size_t		shm_segsz;	 
	long			shm_atime;	 
	long			shm_dtime;	 
	long			shm_ctime;	 
	__kernel_pid_t		shm_cpid;	 
	__kernel_pid_t		shm_lpid;	 
	unsigned long		shm_nattch;	 
	unsigned long		__unused1;
	unsigned long		__unused2;
};
#else
struct shmid64_ds {
	struct ipc64_perm	shm_perm;	 
	__kernel_size_t		shm_segsz;	 
	unsigned long		shm_atime;	 
	unsigned long		shm_dtime;	 
	unsigned long		shm_ctime;	 
	__kernel_pid_t		shm_cpid;	 
	__kernel_pid_t		shm_lpid;	 
	unsigned long		shm_nattch;	 
	unsigned short		shm_atime_high;
	unsigned short		shm_dtime_high;
	unsigned short		shm_ctime_high;
	unsigned short		__unused1;
};
#endif
struct shminfo64 {
	unsigned long	shmmax;
	unsigned long	shmmin;
	unsigned long	shmmni;
	unsigned long	shmseg;
	unsigned long	shmall;
	unsigned long	__unused1;
	unsigned long	__unused2;
	unsigned long	__unused3;
	unsigned long	__unused4;
};
#endif  
