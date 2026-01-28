#ifndef _ASM_MSGBUF_H
#define _ASM_MSGBUF_H
#include <asm/ipcbuf.h>
#if defined(__mips64)
struct msqid64_ds {
	struct ipc64_perm msg_perm;
	long msg_stime;			 
	long msg_rtime;			 
	long msg_ctime;			 
	unsigned long  msg_cbytes;	 
	unsigned long  msg_qnum;	 
	unsigned long  msg_qbytes;	 
	__kernel_pid_t msg_lspid;	 
	__kernel_pid_t msg_lrpid;	 
	unsigned long  __unused4;
	unsigned long  __unused5;
};
#elif defined (__MIPSEB__)
struct msqid64_ds {
	struct ipc64_perm msg_perm;
	unsigned long  msg_stime_high;
	unsigned long  msg_stime;	 
	unsigned long  msg_rtime_high;
	unsigned long  msg_rtime;	 
	unsigned long  msg_ctime_high;
	unsigned long  msg_ctime;	 
	unsigned long  msg_cbytes;	 
	unsigned long  msg_qnum;	 
	unsigned long  msg_qbytes;	 
	__kernel_pid_t msg_lspid;	 
	__kernel_pid_t msg_lrpid;	 
	unsigned long  __unused4;
	unsigned long  __unused5;
};
#elif defined (__MIPSEL__)
struct msqid64_ds {
	struct ipc64_perm msg_perm;
	unsigned long  msg_stime;	 
	unsigned long  msg_stime_high;
	unsigned long  msg_rtime;	 
	unsigned long  msg_rtime_high;
	unsigned long  msg_ctime;	 
	unsigned long  msg_ctime_high;
	unsigned long  msg_cbytes;	 
	unsigned long  msg_qnum;	 
	unsigned long  msg_qbytes;	 
	__kernel_pid_t msg_lspid;	 
	__kernel_pid_t msg_lrpid;	 
	unsigned long  __unused4;
	unsigned long  __unused5;
};
#else
#warning no endianess set
#endif
#endif  
