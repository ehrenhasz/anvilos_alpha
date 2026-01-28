#ifndef RXE_TASK_H
#define RXE_TASK_H
enum {
	TASK_STATE_IDLE		= 0,
	TASK_STATE_BUSY		= 1,
	TASK_STATE_ARMED	= 2,
	TASK_STATE_DRAINING	= 3,
	TASK_STATE_DRAINED	= 4,
	TASK_STATE_INVALID	= 5,
};
struct rxe_task {
	struct work_struct	work;
	int			state;
	spinlock_t		lock;
	struct rxe_qp		*qp;
	int			(*func)(struct rxe_qp *qp);
	int			ret;
	long			num_sched;
	long			num_done;
};
int rxe_alloc_wq(void);
void rxe_destroy_wq(void);
int rxe_init_task(struct rxe_task *task, struct rxe_qp *qp,
		  int (*func)(struct rxe_qp *));
void rxe_cleanup_task(struct rxe_task *task);
void rxe_run_task(struct rxe_task *task);
void rxe_sched_task(struct rxe_task *task);
void rxe_disable_task(struct rxe_task *task);
void rxe_enable_task(struct rxe_task *task);
#endif  
