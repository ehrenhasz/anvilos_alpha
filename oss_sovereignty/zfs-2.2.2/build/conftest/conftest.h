
		#if !defined(_CONFTEST_H) || defined(TRACE_HEADER_MULTI_READ)
		#define _CONFTEST_H

		#undef  TRACE_SYSTEM
		#define TRACE_SYSTEM zfs
		#include <linux/tracepoint.h>

		DECLARE_EVENT_CLASS(zfs_autoconf_event_class,
			TP_PROTO(unsigned long i),
			TP_ARGS(i),
			TP_STRUCT__entry(
				__field(unsigned long, i)
			),
			TP_fast_assign(
				__entry->i = i;
			),
			TP_printk("i = %lu", __entry->i)
		);

		#define DEFINE_AUTOCONF_EVENT(name) 		DEFINE_EVENT(zfs_autoconf_event_class, name, 			TP_PROTO(unsigned long i), 			TP_ARGS(i))
		DEFINE_AUTOCONF_EVENT(zfs_autoconf_event_one);
		DEFINE_AUTOCONF_EVENT(zfs_autoconf_event_two);

		#endif /* _CONFTEST_H */

		#undef  TRACE_INCLUDE_PATH
		#define TRACE_INCLUDE_PATH .
		#define TRACE_INCLUDE_FILE conftest
		#include <trace/define_trace.h>

