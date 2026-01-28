#ifndef IWCM_H
#define IWCM_H
enum iw_cm_state {
	IW_CM_STATE_IDLE,              
	IW_CM_STATE_LISTEN,            
	IW_CM_STATE_CONN_RECV,         
	IW_CM_STATE_CONN_SENT,         
	IW_CM_STATE_ESTABLISHED,       
	IW_CM_STATE_CLOSING,	       
	IW_CM_STATE_DESTROYING         
};
struct iwcm_id_private {
	struct iw_cm_id	id;
	enum iw_cm_state state;
	unsigned long flags;
	struct ib_qp *qp;
	struct completion destroy_comp;
	wait_queue_head_t connect_wait;
	struct list_head work_list;
	spinlock_t lock;
	refcount_t refcount;
	struct list_head work_free_list;
};
#define IWCM_F_DROP_EVENTS	  1
#define IWCM_F_CONNECT_WAIT       2
#endif  
