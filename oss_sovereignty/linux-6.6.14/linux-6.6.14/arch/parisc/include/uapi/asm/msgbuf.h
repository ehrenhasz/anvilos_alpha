#ifndef _PARISC_MSGBUF_H
#define _PARISC_MSGBUF_H
#include <asm/bitsperlong.h>
#include <asm/ipcbuf.h>
struct msqid64_ds {
	struct ipc64_perm msg_perm;
#if __BITS_PER_LONG == 64
	long		 msg_stime;	 
	long		 msg_rtime;	 
	long		 msg_ctime;	 
#else
	unsigned long	msg_stime_high;
	unsigned long	msg_stime;	 
	unsigned long	msg_rtime_high;
	unsigned long	msg_rtime;	 
	unsigned long	msg_ctime_high;
	unsigned long	msg_ctime;	 
#endif
	unsigned long	msg_cbytes;	 
	unsigned long	msg_qnum;	 
	unsigned long	msg_qbytes;	 
	__kernel_pid_t	msg_lspid;	 
	__kernel_pid_t	msg_lrpid;	 
	unsigned long	__unused1;
	unsigned long	__unused2;
};
#endif  
