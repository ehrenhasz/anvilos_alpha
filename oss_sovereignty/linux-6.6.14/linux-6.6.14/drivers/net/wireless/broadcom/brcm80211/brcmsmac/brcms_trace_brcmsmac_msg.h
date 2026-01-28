#if !defined(__TRACE_BRCMSMAC_MSG_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_BRCMSMAC_MSG_H
#include <linux/tracepoint.h>
#undef TRACE_SYSTEM
#define TRACE_SYSTEM brcmsmac_msg
#define MAX_MSG_LEN	100
#pragma GCC diagnostic push
#ifndef __clang__
#pragma GCC diagnostic ignored "-Wsuggest-attribute=format"
#endif
DECLARE_EVENT_CLASS(brcms_msg_event,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf),
	TP_STRUCT__entry(
		__vstring(msg, vaf->fmt, vaf->va)
	),
	TP_fast_assign(
		__assign_vstr(msg, vaf->fmt, vaf->va);
	),
	TP_printk("%s", __get_str(msg))
);
DEFINE_EVENT(brcms_msg_event, brcms_info,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);
DEFINE_EVENT(brcms_msg_event, brcms_warn,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);
DEFINE_EVENT(brcms_msg_event, brcms_err,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);
DEFINE_EVENT(brcms_msg_event, brcms_crit,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);
TRACE_EVENT(brcms_dbg,
	TP_PROTO(u32 level, const char *func, struct va_format *vaf),
	TP_ARGS(level, func, vaf),
	TP_STRUCT__entry(
		__field(u32, level)
		__string(func, func)
		__vstring(msg, vaf->fmt, vaf->va)
	),
	TP_fast_assign(
		__entry->level = level;
		__assign_str(func, func);
		__assign_vstr(msg, vaf->fmt, vaf->va);
	),
	TP_printk("%s: %s", __get_str(func), __get_str(msg))
);
#pragma GCC diagnostic pop
#endif  
#ifdef CONFIG_BRCM_TRACING
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE brcms_trace_brcmsmac_msg
#include <trace/define_trace.h>
#endif  
