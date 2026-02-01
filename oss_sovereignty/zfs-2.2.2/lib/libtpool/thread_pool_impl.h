 

 

#ifndef _THREAD_POOL_IMPL_H
#define	_THREAD_POOL_IMPL_H

#include <thread_pool.h>

#ifdef	__cplusplus
extern "C" {
#endif

 

 
typedef struct tpool_job tpool_job_t;
struct tpool_job {
	tpool_job_t	*tpj_next;		 
	void		(*tpj_func)(void *);	 
	void		*tpj_arg;		 
};

 
typedef struct tpool_active tpool_active_t;
struct tpool_active {
	tpool_active_t	*tpa_next;	 
	pthread_t	tpa_tid;	 
};

 
struct tpool {
	tpool_t		*tp_forw;	 
	tpool_t		*tp_back;
	pthread_mutex_t	tp_mutex;	 
	pthread_cond_t	tp_busycv;	 
	pthread_cond_t	tp_workcv;	 
	pthread_cond_t	tp_waitcv;	 
	tpool_active_t	*tp_active;	 
	tpool_job_t	*tp_head;	 
	tpool_job_t	*tp_tail;
	pthread_attr_t	tp_attr;	 
	int		tp_flags;	 
	uint_t		tp_linger;	 
	int		tp_njobs;	 
	int		tp_minimum;	 
	int		tp_maximum;	 
	int		tp_current;	 
	int		tp_idle;	 
};

 
#define	TP_WAIT		0x01		 
#define	TP_SUSPEND	0x02		 
#define	TP_DESTROY	0x04		 
#define	TP_ABANDON	0x08		 

#ifdef	__cplusplus
}
#endif

#endif  
