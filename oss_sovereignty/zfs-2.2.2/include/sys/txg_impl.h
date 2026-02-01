 

 

 

#ifndef _SYS_TXG_IMPL_H
#define	_SYS_TXG_IMPL_H

#include <sys/spa.h>
#include <sys/txg.h>

#ifdef	__cplusplus
extern "C" {
#endif

 
struct tx_cpu {
	kmutex_t	tc_open_lock;	 
	kmutex_t	tc_lock;	 
	kcondvar_t	tc_cv[TXG_SIZE];
	uint64_t	tc_count[TXG_SIZE];	 
	list_t		tc_callbacks[TXG_SIZE];  
} ____cacheline_aligned;

 
typedef struct tx_state {
	tx_cpu_t	*tx_cpu;	 
	kmutex_t	tx_sync_lock;	 

	uint64_t	tx_open_txg;	 
	uint64_t	tx_quiescing_txg;  
	uint64_t	tx_quiesced_txg;  
	uint64_t	tx_syncing_txg;	 
	uint64_t	tx_synced_txg;	 

	hrtime_t	tx_open_time;	 

	uint64_t	tx_sync_txg_waiting;  
	uint64_t	tx_quiesce_txg_waiting;  

	kcondvar_t	tx_sync_more_cv;
	kcondvar_t	tx_sync_done_cv;
	kcondvar_t	tx_quiesce_more_cv;
	kcondvar_t	tx_quiesce_done_cv;
	kcondvar_t	tx_timeout_cv;
	kcondvar_t	tx_exit_cv;	 

	uint8_t		tx_threads;	 
	uint8_t		tx_exiting;	 

	kthread_t	*tx_sync_thread;
	kthread_t	*tx_quiesce_thread;

	taskq_t		*tx_commit_cb_taskq;  
} tx_state_t;

#ifdef	__cplusplus
}
#endif

#endif	 
