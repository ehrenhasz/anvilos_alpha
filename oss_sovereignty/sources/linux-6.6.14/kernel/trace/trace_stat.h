
#ifndef __TRACE_STAT_H
#define __TRACE_STAT_H

#include <linux/seq_file.h>


struct tracer_stat {
	
	const char		*name;
	
	void			*(*stat_start)(struct tracer_stat *trace);
	void			*(*stat_next)(void *prev, int idx);
	
	cmp_func_t		stat_cmp;
	
	int			(*stat_show)(struct seq_file *s, void *p);
	
	void			(*stat_release)(void *stat);
	
	int			(*stat_headers)(struct seq_file *s);
};


extern int register_stat_tracer(struct tracer_stat *trace);
extern void unregister_stat_tracer(struct tracer_stat *trace);

#endif 
