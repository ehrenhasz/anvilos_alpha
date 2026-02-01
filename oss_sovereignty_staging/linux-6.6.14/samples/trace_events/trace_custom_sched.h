 

 
#if !defined(_TRACE_CUSTOM_SCHED_H) || defined(TRACE_CUSTOM_MULTI_READ)
#define _TRACE_CUSTOM_SCHED_H

 
#include <linux/trace_events.h>

 
TRACE_CUSTOM_EVENT(sched_switch,

	 
	TP_PROTO(bool preempt,
		 struct task_struct *prev,
		 struct task_struct *next,
		 unsigned int prev_state),

	TP_ARGS(preempt, prev, next, prev_state),

	 
	TP_STRUCT__entry(
		__field(	unsigned short,		prev_prio	)
		__field(	unsigned short,		next_prio	)
		__field(	pid_t,	next_pid			)
	),

	TP_fast_assign(
		__entry->prev_prio	= prev->prio;
		__entry->next_pid	= next->pid;
		__entry->next_prio	= next->prio;
	),

	TP_printk("prev_prio=%d next_pid=%d next_prio=%d",
		  __entry->prev_prio, __entry->next_pid, __entry->next_prio)
)


TRACE_CUSTOM_EVENT(sched_waking,

	TP_PROTO(struct task_struct *p),

	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(	pid_t,			pid	)
		__field(	unsigned short,		prio	)
	),

	TP_fast_assign(
		__entry->pid	= p->pid;
		__entry->prio	= p->prio;
	),

	TP_printk("pid=%d prio=%d", __entry->pid, __entry->prio)
)
#endif
 

 
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .

 
#define TRACE_INCLUDE_FILE trace_custom_sched
#include <trace/define_custom_trace.h>
