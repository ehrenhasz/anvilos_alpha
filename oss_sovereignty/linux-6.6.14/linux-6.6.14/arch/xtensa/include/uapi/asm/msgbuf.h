#ifndef _XTENSA_MSGBUF_H
#define _XTENSA_MSGBUF_H
#include <asm/ipcbuf.h>
struct msqid64_ds {
	struct ipc64_perm msg_perm;
#ifdef __XTENSA_EB__
	unsigned long  msg_stime_high;
	unsigned long  msg_stime;	 
	unsigned long  msg_rtime_high;
	unsigned long  msg_rtime;	 
	unsigned long  msg_ctime_high;
	unsigned long  msg_ctime;	 
#elif defined(__XTENSA_EL__)
	unsigned long  msg_stime;	 
	unsigned long  msg_stime_high;
	unsigned long  msg_rtime;	 
	unsigned long  msg_rtime_high;
	unsigned long  msg_ctime;	 
	unsigned long  msg_ctime_high;
#else
# error processor byte order undefined!
#endif
	unsigned long  msg_cbytes;	 
	unsigned long  msg_qnum;	 
	unsigned long  msg_qbytes;	 
	__kernel_pid_t msg_lspid;	 
	__kernel_pid_t msg_lrpid;	 
	unsigned long  __unused4;
	unsigned long  __unused5;
};
#endif	 
