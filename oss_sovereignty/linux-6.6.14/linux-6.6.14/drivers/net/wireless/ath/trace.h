#if !defined(_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_H
#include <linux/tracepoint.h>
#include "ath.h"
#undef TRACE_SYSTEM
#define TRACE_SYSTEM ath
#if !defined(CONFIG_ATH_TRACEPOINTS)
#undef TRACE_EVENT
#define TRACE_EVENT(name, proto, ...) static inline void trace_ ## name(proto) {}
#endif  
TRACE_EVENT(ath_log,
	    TP_PROTO(struct wiphy *wiphy,
		     struct va_format *vaf),
	    TP_ARGS(wiphy, vaf),
	    TP_STRUCT__entry(
		    __string(device, wiphy_name(wiphy))
		    __string(driver, KBUILD_MODNAME)
		    __vstring(msg, vaf->fmt, vaf->va)
	    ),
	    TP_fast_assign(
		    __assign_str(device, wiphy_name(wiphy));
		    __assign_str(driver, KBUILD_MODNAME);
		    __assign_vstr(msg, vaf->fmt, vaf->va);
	    ),
	    TP_printk(
		    "%s %s %s",
		    __get_str(driver),
		    __get_str(device),
		    __get_str(msg)
	    )
);
#endif  
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE trace
#include <trace/define_trace.h>
