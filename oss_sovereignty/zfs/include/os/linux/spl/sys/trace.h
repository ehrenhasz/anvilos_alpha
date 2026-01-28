#if defined(_KERNEL)
#if defined(HAVE_DECLARE_EVENT_CLASS)
#undef TRACE_SYSTEM
#define	TRACE_SYSTEM zfs
#if !defined(_TRACE_ZFS_H) || defined(TRACE_HEADER_MULTI_READ)
#define	_TRACE_ZFS_H
#include <linux/tracepoint.h>
#include <sys/types.h>
#define	DTRACE_PROBE(name) \
	((void)0)
#define	DTRACE_PROBE1(name, t1, arg1) \
	trace_zfs_##name((arg1))
#define	DTRACE_PROBE2(name, t1, arg1, t2, arg2) \
	trace_zfs_##name((arg1), (arg2))
#define	DTRACE_PROBE3(name, t1, arg1, t2, arg2, t3, arg3) \
	trace_zfs_##name((arg1), (arg2), (arg3))
#define	DTRACE_PROBE4(name, t1, arg1, t2, arg2, t3, arg3, t4, arg4) \
	trace_zfs_##name((arg1), (arg2), (arg3), (arg4))
#endif  
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define	TRACE_INCLUDE_PATH sys
#define	TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
#else  
#define	DTRACE_PROBE(name) \
	trace_zfs_##name()
#define	DTRACE_PROBE1(name, t1, arg1) \
	trace_zfs_##name((uintptr_t)(arg1))
#define	DTRACE_PROBE2(name, t1, arg1, t2, arg2) \
	trace_zfs_##name((uintptr_t)(arg1), (uintptr_t)(arg2))
#define	DTRACE_PROBE3(name, t1, arg1, t2, arg2, t3, arg3) \
	trace_zfs_##name((uintptr_t)(arg1), (uintptr_t)(arg2), \
	(uintptr_t)(arg3))
#define	DTRACE_PROBE4(name, t1, arg1, t2, arg2, t3, arg3, t4, arg4) \
	trace_zfs_##name((uintptr_t)(arg1), (uintptr_t)(arg2), \
	(uintptr_t)(arg3), (uintptr_t)(arg4))
#define	PROTO_DTRACE_PROBE(name)				\
	noinline void trace_zfs_##name(void)
#define	PROTO_DTRACE_PROBE1(name)				\
	noinline void trace_zfs_##name(uintptr_t)
#define	PROTO_DTRACE_PROBE2(name)				\
	noinline void trace_zfs_##name(uintptr_t, uintptr_t)
#define	PROTO_DTRACE_PROBE3(name)				\
	noinline void trace_zfs_##name(uintptr_t, uintptr_t,	\
	uintptr_t)
#define	PROTO_DTRACE_PROBE4(name)				\
	noinline void trace_zfs_##name(uintptr_t, uintptr_t,	\
	uintptr_t, uintptr_t)
#if defined(CREATE_TRACE_POINTS)
#define	FUNC_DTRACE_PROBE(name)					\
PROTO_DTRACE_PROBE(name);					\
noinline void trace_zfs_##name(void) { }			\
EXPORT_SYMBOL(trace_zfs_##name)
#define	FUNC_DTRACE_PROBE1(name)				\
PROTO_DTRACE_PROBE1(name);					\
noinline void trace_zfs_##name(uintptr_t arg1) { }		\
EXPORT_SYMBOL(trace_zfs_##name)
#define	FUNC_DTRACE_PROBE2(name)				\
PROTO_DTRACE_PROBE2(name);					\
noinline void trace_zfs_##name(uintptr_t arg1,			\
    uintptr_t arg2) { }						\
EXPORT_SYMBOL(trace_zfs_##name)
#define	FUNC_DTRACE_PROBE3(name)				\
PROTO_DTRACE_PROBE3(name);					\
noinline void trace_zfs_##name(uintptr_t arg1,			\
    uintptr_t arg2, uintptr_t arg3) { }				\
EXPORT_SYMBOL(trace_zfs_##name)
#define	FUNC_DTRACE_PROBE4(name)				\
PROTO_DTRACE_PROBE4(name);					\
noinline void trace_zfs_##name(uintptr_t arg1,			\
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4) { }		\
EXPORT_SYMBOL(trace_zfs_##name)
#undef	DEFINE_DTRACE_PROBE
#define	DEFINE_DTRACE_PROBE(name)	FUNC_DTRACE_PROBE(name)
#undef	DEFINE_DTRACE_PROBE1
#define	DEFINE_DTRACE_PROBE1(name)	FUNC_DTRACE_PROBE1(name)
#undef	DEFINE_DTRACE_PROBE2
#define	DEFINE_DTRACE_PROBE2(name)	FUNC_DTRACE_PROBE2(name)
#undef	DEFINE_DTRACE_PROBE3
#define	DEFINE_DTRACE_PROBE3(name)	FUNC_DTRACE_PROBE3(name)
#undef	DEFINE_DTRACE_PROBE4
#define	DEFINE_DTRACE_PROBE4(name)	FUNC_DTRACE_PROBE4(name)
#else  
#define	DEFINE_DTRACE_PROBE(name)	PROTO_DTRACE_PROBE(name)
#define	DEFINE_DTRACE_PROBE1(name)	PROTO_DTRACE_PROBE1(name)
#define	DEFINE_DTRACE_PROBE2(name)	PROTO_DTRACE_PROBE2(name)
#define	DEFINE_DTRACE_PROBE3(name)	PROTO_DTRACE_PROBE3(name)
#define	DEFINE_DTRACE_PROBE4(name)	PROTO_DTRACE_PROBE4(name)
#endif  
#endif  
#endif  
