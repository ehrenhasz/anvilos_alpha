 
 
 
 

#undef TRACE_SYSTEM
#define TRACE_SYSTEM e1000e_trace

#if !defined(_TRACE_E1000E_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_E1000E_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(e1000e_trace_mac_register,
	    TP_PROTO(uint32_t reg),
	    TP_ARGS(reg),
	    TP_STRUCT__entry(__field(uint32_t,	reg)),
	    TP_fast_assign(__entry->reg = reg;),
	    TP_printk("event: TraceHub e1000e mac register: 0x%08x",
		      __entry->reg)
);

#endif
 
 

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE e1000e_trace

#include <trace/define_trace.h>
