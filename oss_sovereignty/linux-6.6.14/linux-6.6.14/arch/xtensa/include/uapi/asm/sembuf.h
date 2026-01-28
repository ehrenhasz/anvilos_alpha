#ifndef _XTENSA_SEMBUF_H
#define _XTENSA_SEMBUF_H
#include <asm/byteorder.h>
#include <asm/ipcbuf.h>
struct semid64_ds {
	struct ipc64_perm sem_perm;		 
#ifdef __XTENSA_EL__
	unsigned long	sem_otime;		 
	unsigned long	sem_otime_high;
	unsigned long	sem_ctime;		 
	unsigned long	sem_ctime_high;
#else
	unsigned long	sem_otime_high;
	unsigned long	sem_otime;		 
	unsigned long	sem_ctime_high;
	unsigned long	sem_ctime;		 
#endif
	unsigned long	sem_nsems;		 
	unsigned long	__unused3;
	unsigned long	__unused4;
};
#endif  
