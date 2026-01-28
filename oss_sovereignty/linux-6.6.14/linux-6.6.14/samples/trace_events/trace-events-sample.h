#undef TRACE_SYSTEM
#define TRACE_SYSTEM sample-trace
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR sample_trace
#if !defined(_TRACE_EVENT_SAMPLE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_SAMPLE_H
#include <linux/tracepoint.h>
#ifndef __TRACE_EVENT_SAMPLE_HELPER_FUNCTIONS
#define __TRACE_EVENT_SAMPLE_HELPER_FUNCTIONS
static inline int __length_of(const int *list)
{
	int i;
	if (!list)
		return 0;
	for (i = 0; list[i]; i++)
		;
	return i;
}
enum {
	TRACE_SAMPLE_FOO = 2,
	TRACE_SAMPLE_BAR = 4,
	TRACE_SAMPLE_ZOO = 8,
};
#endif
TRACE_DEFINE_ENUM(TRACE_SAMPLE_FOO);
TRACE_DEFINE_ENUM(TRACE_SAMPLE_BAR);
TRACE_DEFINE_ENUM(TRACE_SAMPLE_ZOO);
TRACE_EVENT(foo_bar,
	TP_PROTO(const char *foo, int bar, const int *lst,
		 const char *string, const struct cpumask *mask,
		 const char *fmt, va_list *va),
	TP_ARGS(foo, bar, lst, string, mask, fmt, va),
	TP_STRUCT__entry(
		__array(	char,	foo,    10		)
		__field(	int,	bar			)
		__dynamic_array(int,	list,   __length_of(lst))
		__string(	str,	string			)
		__bitmask(	cpus,	num_possible_cpus()	)
		__cpumask(	cpum				)
		__vstring(	vstr,	fmt,	va		)
	),
	TP_fast_assign(
		strlcpy(__entry->foo, foo, 10);
		__entry->bar	= bar;
		memcpy(__get_dynamic_array(list), lst,
		       __length_of(lst) * sizeof(int));
		__assign_str(str, string);
		__assign_vstr(vstr, fmt, va);
		__assign_bitmask(cpus, cpumask_bits(mask), num_possible_cpus());
		__assign_cpumask(cpum, cpumask_bits(mask));
	),
	TP_printk("foo %s %d %s %s %s %s (%s) (%s) %s", __entry->foo, __entry->bar,
		  __print_symbolic(__entry->bar,
				   { 0, "zero" },
				   { TRACE_SAMPLE_FOO, "TWO" },
				   { TRACE_SAMPLE_BAR, "FOUR" },
				   { TRACE_SAMPLE_ZOO, "EIGHT" },
				   { 10, "TEN" }
			  ),
		  __print_flags(__entry->bar, "|",
				{ 1, "BIT1" },
				{ 2, "BIT2" },
				{ 4, "BIT3" },
				{ 8, "BIT4" }
			  ),
		  __print_array(__get_dynamic_array(list),
				__get_dynamic_array_len(list) / sizeof(int),
				sizeof(int)),
		  __get_str(str), __get_bitmask(cpus), __get_cpumask(cpum),
		  __get_str(vstr))
);
TRACE_EVENT_CONDITION(foo_bar_with_cond,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	TP_CONDITION(!(bar % 10)),
	TP_STRUCT__entry(
		__string(	foo,    foo		)
		__field(	int,	bar			)
	),
	TP_fast_assign(
		__assign_str(foo, foo);
		__entry->bar	= bar;
	),
	TP_printk("foo %s %d", __get_str(foo), __entry->bar)
);
int foo_bar_reg(void);
void foo_bar_unreg(void);
TRACE_EVENT_FN(foo_bar_with_fn,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	TP_STRUCT__entry(
		__string(	foo,    foo		)
		__field(	int,	bar		)
	),
	TP_fast_assign(
		__assign_str(foo, foo);
		__entry->bar	= bar;
	),
	TP_printk("foo %s %d", __get_str(foo), __entry->bar),
	foo_bar_reg, foo_bar_unreg
);
DECLARE_EVENT_CLASS(foo_template,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	TP_STRUCT__entry(
		__string(	foo,    foo		)
		__field(	int,	bar		)
	),
	TP_fast_assign(
		__assign_str(foo, foo);
		__entry->bar	= bar;
	),
	TP_printk("foo %s %d", __get_str(foo), __entry->bar)
);
DEFINE_EVENT(foo_template, foo_with_template_simple,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar));
DEFINE_EVENT_CONDITION(foo_template, foo_with_template_cond,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	TP_CONDITION(!(bar % 8)));
DEFINE_EVENT_FN(foo_template, foo_with_template_fn,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	foo_bar_reg, foo_bar_unreg);
DEFINE_EVENT_PRINT(foo_template, foo_with_template_print,
	TP_PROTO(const char *foo, int bar),
	TP_ARGS(foo, bar),
	TP_printk("bar %s %d", __get_str(foo), __entry->bar));
TRACE_EVENT(foo_rel_loc,
	TP_PROTO(const char *foo, int bar, unsigned long *mask, const cpumask_t *cpus),
	TP_ARGS(foo, bar, mask, cpus),
	TP_STRUCT__entry(
		__rel_string(	foo,	foo	)
		__field(	int,	bar	)
		__rel_bitmask(	bitmask,
			BITS_PER_BYTE * sizeof(unsigned long)	)
		__rel_cpumask(	cpumask )
	),
	TP_fast_assign(
		__assign_rel_str(foo, foo);
		__entry->bar = bar;
		__assign_rel_bitmask(bitmask, mask,
			BITS_PER_BYTE * sizeof(unsigned long));
		__assign_rel_cpumask(cpumask, cpus);
	),
	TP_printk("foo_rel_loc %s, %d, %s, %s", __get_rel_str(foo), __entry->bar,
		  __get_rel_bitmask(bitmask),
		  __get_rel_cpumask(cpumask))
);
#endif
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace-events-sample
#include <trace/define_trace.h>
