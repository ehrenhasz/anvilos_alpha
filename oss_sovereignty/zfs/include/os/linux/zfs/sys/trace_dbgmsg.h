#if defined(_KERNEL)
#if defined(HAVE_DECLARE_EVENT_CLASS)
#undef TRACE_SYSTEM
#define	TRACE_SYSTEM zfs
#undef TRACE_SYSTEM_VAR
#define	TRACE_SYSTEM_VAR zfs_dbgmsg
#if !defined(_TRACE_DBGMSG_H) || defined(TRACE_HEADER_MULTI_READ)
#define	_TRACE_DBGMSG_H
#include <linux/tracepoint.h>
DECLARE_EVENT_CLASS(zfs_dprintf_class,
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
	    __string(msg, msg)
	),
	TP_fast_assign(
	    __assign_str(msg, msg);
	),
	TP_printk("%s", __get_str(msg))
);
#define	DEFINE_DPRINTF_EVENT(name) \
DEFINE_EVENT(zfs_dprintf_class, name, \
    TP_PROTO(const char *msg), \
    TP_ARGS(msg))
DEFINE_DPRINTF_EVENT(zfs_zfs__dprintf);
#endif  
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define	TRACE_INCLUDE_PATH sys
#define	TRACE_INCLUDE_FILE trace_dbgmsg
#include <trace/define_trace.h>
#else
DEFINE_DTRACE_PROBE1(zfs__dprintf);
#endif  
#endif  
