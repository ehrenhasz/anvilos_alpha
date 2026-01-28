#ifndef _PARISC_SEMBUF_H
#define _PARISC_SEMBUF_H
#include <asm/bitsperlong.h>
#include <asm/ipcbuf.h>
struct semid64_ds {
	struct ipc64_perm sem_perm;		 
#if __BITS_PER_LONG == 64
	long		sem_otime;		 
	long		sem_ctime;		 
#else
	unsigned long	sem_otime_high;
	unsigned long	sem_otime;		 
	unsigned long	sem_ctime_high;
	unsigned long	sem_ctime;		 
#endif
	unsigned long	sem_nsems;		 
	unsigned long	__unused1;
	unsigned long	__unused2;
};
#endif  
