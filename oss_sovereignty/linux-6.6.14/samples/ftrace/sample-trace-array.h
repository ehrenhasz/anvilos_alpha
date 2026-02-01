 

 
#undef TRACE_SYSTEM
#define TRACE_SYSTEM sample-subsystem

 
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR sample_subsystem

 

 
#if !defined(_SAMPLE_TRACE_ARRAY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SAMPLE_TRACE_ARRAY_H

#include <linux/tracepoint.h>
TRACE_EVENT(sample_event,

	TP_PROTO(int count, unsigned long time),

	TP_ARGS(count, time),

	TP_STRUCT__entry(
		__field(int, count)
		__field(unsigned long, time)
	),

	TP_fast_assign(
		__entry->count = count;
		__entry->time = time;
	),

	TP_printk("count value=%d at jiffies=%lu", __entry->count,
		__entry->time)
	);
#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE sample-trace-array
#include <trace/define_trace.h>
