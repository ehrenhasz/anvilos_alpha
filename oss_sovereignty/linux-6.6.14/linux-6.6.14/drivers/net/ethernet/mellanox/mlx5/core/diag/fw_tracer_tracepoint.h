#if !defined(__LIB_TRACER_TRACEPOINT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __LIB_TRACER_TRACEPOINT_H__
#include <linux/tracepoint.h>
#include "fw_tracer.h"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mlx5
TRACE_EVENT(mlx5_fw,
	TP_PROTO(const struct mlx5_fw_tracer *tracer, u64 trace_timestamp,
		 bool lost, u8 event_id, const char *msg),
	TP_ARGS(tracer, trace_timestamp, lost, event_id, msg),
	TP_STRUCT__entry(
		__string(dev_name, dev_name(tracer->dev->device))
		__field(u64, trace_timestamp)
		__field(bool, lost)
		__field(u8, event_id)
		__string(msg, msg)
	),
	TP_fast_assign(
		__assign_str(dev_name,
			     dev_name(tracer->dev->device));
		__entry->trace_timestamp = trace_timestamp;
		__entry->lost = lost;
		__entry->event_id = event_id;
		__assign_str(msg, msg);
	),
	TP_printk("%s [0x%llx] %d [0x%x] %s",
		  __get_str(dev_name),
		  __entry->trace_timestamp,
		  __entry->lost, __entry->event_id,
		  __get_str(msg))
);
#endif
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH ./diag
#define TRACE_INCLUDE_FILE fw_tracer_tracepoint
#include <trace/define_trace.h>
