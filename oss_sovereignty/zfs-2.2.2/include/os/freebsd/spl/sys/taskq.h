 
 

#ifndef	_SYS_TASKQ_H
#define	_SYS_TASKQ_H

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/thread.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TASKQ_NAMELEN	31

typedef struct taskq {
	struct taskqueue	*tq_queue;
} taskq_t;

typedef uintptr_t taskqid_t;
typedef void (task_func_t)(void *);

typedef struct taskq_ent {
	union {
		struct task	 tqent_task;
		struct timeout_task tqent_timeout_task;
	};
	task_func_t	*tqent_func;
	void		*tqent_arg;
	taskqid_t	 tqent_id;
	LIST_ENTRY(taskq_ent) tqent_hash;
	uint_t		 tqent_type;
	volatile uint_t	 tqent_rc;
} taskq_ent_t;

 
#define	TASKQ_PREPOPULATE	0x0001	 
#define	TASKQ_CPR_SAFE		0x0002	 
#define	TASKQ_DYNAMIC		0x0004	 
#define	TASKQ_THREADS_CPU_PCT	0x0008	 
#define	TASKQ_DC_BATCH		0x0010	 

 
#define	TQ_SLEEP	0x00	 
#define	TQ_NOSLEEP	0x01	 
#define	TQ_NOQUEUE	0x02	 
#define	TQ_NOALLOC	0x04	 
#define	TQ_FRONT	0x08	 

#define	TASKQID_INVALID		((taskqid_t)0)

#define	taskq_init_ent(x)
extern taskq_t *system_taskq;
 
extern taskq_t *system_delay_taskq;

extern taskqid_t taskq_dispatch(taskq_t *, task_func_t, void *, uint_t);
extern taskqid_t taskq_dispatch_delay(taskq_t *, task_func_t, void *,
    uint_t, clock_t);
extern void taskq_dispatch_ent(taskq_t *, task_func_t, void *, uint_t,
    taskq_ent_t *);
extern int taskq_empty_ent(taskq_ent_t *);
taskq_t	*taskq_create(const char *, int, pri_t, int, int, uint_t);
taskq_t	*taskq_create_instance(const char *, int, int, pri_t, int, int, uint_t);
taskq_t	*taskq_create_proc(const char *, int, pri_t, int, int,
    struct proc *, uint_t);
taskq_t	*taskq_create_sysdc(const char *, int, int, int,
    struct proc *, uint_t, uint_t);
void	nulltask(void *);
extern void taskq_destroy(taskq_t *);
extern void taskq_wait_id(taskq_t *, taskqid_t);
extern void taskq_wait_outstanding(taskq_t *, taskqid_t);
extern void taskq_wait(taskq_t *);
extern int taskq_cancel_id(taskq_t *, taskqid_t);
extern int taskq_member(taskq_t *, kthread_t *);
extern taskq_t *taskq_of_curthread(void);
void	taskq_suspend(taskq_t *);
int	taskq_suspended(taskq_t *);
void	taskq_resume(taskq_t *);

#ifdef	__cplusplus
}
#endif

#endif  

#ifdef _STANDALONE
typedef int taskq_ent_t;
#define	taskq_init_ent(x)
#endif  

#endif	 
