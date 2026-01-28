#if !defined(__TRACE_BRCMSMAC_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_BRCMSMAC_H
#include <linux/tracepoint.h>
#undef TRACE_SYSTEM
#define TRACE_SYSTEM brcmsmac
TRACE_EVENT(brcms_timer,
	TP_PROTO(struct brcms_timer *t),
	TP_ARGS(t),
	TP_STRUCT__entry(
		__field(uint, ms)
		__field(uint, set)
		__field(uint, periodic)
	),
	TP_fast_assign(
		__entry->ms = t->ms;
		__entry->set = t->set;
		__entry->periodic = t->periodic;
	),
	TP_printk(
		"ms=%u set=%u periodic=%u",
		__entry->ms, __entry->set, __entry->periodic
	)
);
TRACE_EVENT(brcms_dpc,
	TP_PROTO(unsigned long data),
	TP_ARGS(data),
	TP_STRUCT__entry(
		__field(unsigned long, data)
	),
	TP_fast_assign(
		__entry->data = data;
	),
	TP_printk(
		"data=%p",
		(void *)__entry->data
	)
);
TRACE_EVENT(brcms_macintstatus,
	TP_PROTO(const struct device *dev, int in_isr, u32 macintstatus,
		 u32 mask),
	TP_ARGS(dev, in_isr, macintstatus, mask),
	TP_STRUCT__entry(
		__string(dev, dev_name(dev))
		__field(int, in_isr)
		__field(u32, macintstatus)
		__field(u32, mask)
	),
	TP_fast_assign(
		__assign_str(dev, dev_name(dev));
		__entry->in_isr = in_isr;
		__entry->macintstatus = macintstatus;
		__entry->mask = mask;
	),
	TP_printk("[%s] in_isr=%d macintstatus=%#x mask=%#x", __get_str(dev),
		  __entry->in_isr, __entry->macintstatus, __entry->mask)
);
#endif  
#ifdef CONFIG_BRCM_TRACING
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE brcms_trace_brcmsmac
#include <trace/define_trace.h>
#endif  
