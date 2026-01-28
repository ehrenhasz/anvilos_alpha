#ifndef _ORADAX_H
#define	_ORADAX_H
#include <linux/types.h>
#define	CCB_KILL 0
#define	CCB_INFO 1
#define	CCB_DEQUEUE 2
struct dax_command {
	__u16 command;		 
	__u16 ca_offset;	 
};
struct ccb_kill_result {
	__u16 action;		 
};
struct ccb_info_result {
	__u16 state;		 
	__u16 inst_num;		 
	__u16 q_num;		 
	__u16 q_pos;		 
};
struct ccb_exec_result {
	__u64	status_data;	 
	__u32	status;		 
};
union ccb_result {
	struct ccb_exec_result exec;
	struct ccb_info_result info;
	struct ccb_kill_result kill;
};
#define	DAX_MMAP_LEN		(16 * 1024)
#define	DAX_MAX_CCBS		15
#define	DAX_CCB_BUF_MAXLEN	(DAX_MAX_CCBS * 64)
#define	DAX_NAME		"oradax"
#define	DAX_SUBMIT_OK			0
#define	DAX_SUBMIT_ERR_RETRY		1
#define	DAX_SUBMIT_ERR_WOULDBLOCK	2
#define	DAX_SUBMIT_ERR_BUSY		3
#define	DAX_SUBMIT_ERR_THR_INIT		4
#define	DAX_SUBMIT_ERR_ARG_INVAL	5
#define	DAX_SUBMIT_ERR_CCB_INVAL	6
#define	DAX_SUBMIT_ERR_NO_CA_AVAIL	7
#define	DAX_SUBMIT_ERR_CCB_ARR_MMU_MISS	8
#define	DAX_SUBMIT_ERR_NOMAP		9
#define	DAX_SUBMIT_ERR_NOACCESS		10
#define	DAX_SUBMIT_ERR_TOOMANY		11
#define	DAX_SUBMIT_ERR_UNAVAIL		12
#define	DAX_SUBMIT_ERR_INTERNAL		13
#define	DAX_CCB_COMPLETED	0
#define	DAX_CCB_ENQUEUED	1
#define	DAX_CCB_INPROGRESS	2
#define	DAX_CCB_NOTFOUND	3
#define	DAX_KILL_COMPLETED	0
#define	DAX_KILL_DEQUEUED	1
#define	DAX_KILL_KILLED		2
#define	DAX_KILL_NOTFOUND	3
#endif  
