#ifndef UTIL_LINUX_H_IRQ_COMMON
#define UTIL_LINUX_H_IRQ_COMMON

#include "c.h"
#include "nls.h"
#include "cpuset.h"


enum {
	COL_IRQ = 0,
	COL_TOTAL,
	COL_DELTA,
	COL_NAME,

	__COL_COUNT
};

struct irq_info {
	char *irq;			
	char *name;			
	unsigned long total;		
	unsigned long delta;		
};

struct irq_cpu {
	unsigned long total;
	unsigned long delta;
};

struct irq_stat {
	unsigned long nr_irq;		
	unsigned long nr_irq_info;	
	struct irq_info *irq_info;	
	struct irq_cpu *cpus;		 
	size_t nr_active_cpu;		
	unsigned long total_irq;	
	unsigned long delta_irq;	
};


typedef int (irq_cmp_t)(const struct irq_info *, const struct irq_info *);


struct irq_output {
	int columns[__COL_COUNT * 2];
	size_t ncolumns;

	irq_cmp_t *sort_cmp_func;

	unsigned int
		json:1,		
		pairs:1,	
		no_headings:1;	
};

int irq_column_name_to_id(char const *const name, size_t const namesz);
void free_irqstat(struct irq_stat *stat);

void irq_print_columns(FILE *f, int nodelta);

void set_sort_func_by_name(struct irq_output *out, const char *name);
void set_sort_func_by_key(struct irq_output *out, const char c);

struct libscols_table *get_scols_table(struct irq_output *out,
                                              struct irq_stat *prev,
                                              struct irq_stat **xstat,
                                              int softirq,
                                              size_t setsize,
                                              cpu_set_t *cpuset);

struct libscols_table *get_scols_cpus_table(struct irq_output *out,
                                        struct irq_stat *prev,
                                        struct irq_stat *curr,
                                        size_t setsize,
                                        cpu_set_t *cpuset);

#endif 
