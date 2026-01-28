
#ifndef _PROBE_EVENT_H
#define _PROBE_EVENT_H

#include <linux/compiler.h>
#include <stdbool.h>

struct intlist;
struct nsinfo;


struct probe_conf {
	bool	show_ext_vars;
	bool	show_location_range;
	bool	force_add;
	bool	no_inlines;
	bool	cache;
	bool	bootconfig;
	int	max_probes;
	unsigned long	magic_num;
};
extern struct probe_conf probe_conf;
extern bool probe_event_dry_run;

#define DEFAULT_PROBE_MAGIC_NUM	0xdeade12d	

struct symbol;


struct probe_trace_point {
	char		*realname;	
	char		*symbol;	
	char		*module;	
	unsigned long	offset;		
	unsigned long	ref_ctr_offset;	
	u64		address;	
	bool		retprobe;	
};


struct probe_trace_arg_ref {
	struct probe_trace_arg_ref	*next;	
	long				offset;	
	bool				user_access;	
};


struct probe_trace_arg {
	char				*name;	
	char				*value;	
	char				*type;	
	struct probe_trace_arg_ref	*ref;	
};


struct probe_trace_event {
	char				*event;	
	char				*group;	
	struct probe_trace_point	point;	
	int				nargs;	
	bool				uprobes;	
	struct probe_trace_arg		*args;	
};


struct perf_probe_point {
	char		*file;		
	char		*function;	
	int		line;		
	bool		retprobe;	
	char		*lazy_line;	
	unsigned long	offset;		
	u64		abs_address;	
};


struct perf_probe_arg_field {
	struct perf_probe_arg_field	*next;	
	char				*name;	
	long				index;	
	bool				ref;	
};


struct perf_probe_arg {
	char				*name;	
	char				*var;	
	char				*type;	
	struct perf_probe_arg_field	*field;	
	bool				user_access;	
};


struct perf_probe_event {
	char			*event;	
	char			*group;	
	struct perf_probe_point	point;	
	int			nargs;	
	bool			sdt;	
	bool			uprobes;	
	char			*target;	
	struct perf_probe_arg	*args;	
	struct probe_trace_event *tevs;
	int			ntevs;
	struct nsinfo		*nsi;	
};


struct line_range {
	char			*file;		
	char			*function;	
	int			start;		
	int			end;		
	int			offset;		
	char			*path;		
	char			*comp_dir;	
	struct intlist		*line_list;	
};

struct strlist;


struct variable_list {
	struct probe_trace_point	point;	
	struct strlist			*vars;	
};

struct map;
int init_probe_symbol_maps(bool user_only);
void exit_probe_symbol_maps(void);


int parse_perf_probe_command(const char *cmd, struct perf_probe_event *pev);
int parse_probe_trace_command(const char *cmd, struct probe_trace_event *tev);


char *synthesize_perf_probe_command(struct perf_probe_event *pev);
char *synthesize_probe_trace_command(struct probe_trace_event *tev);
char *synthesize_perf_probe_arg(struct perf_probe_arg *pa);

int perf_probe_event__copy(struct perf_probe_event *dst,
			   struct perf_probe_event *src);

bool perf_probe_with_var(struct perf_probe_event *pev);


bool perf_probe_event_need_dwarf(struct perf_probe_event *pev);


void clear_perf_probe_event(struct perf_probe_event *pev);
void clear_probe_trace_event(struct probe_trace_event *tev);


int parse_line_range_desc(const char *cmd, struct line_range *lr);


void line_range__clear(struct line_range *lr);


int line_range__init(struct line_range *lr);

int add_perf_probe_events(struct perf_probe_event *pevs, int npevs);
int convert_perf_probe_events(struct perf_probe_event *pevs, int npevs);
int apply_perf_probe_events(struct perf_probe_event *pevs, int npevs);
int show_probe_trace_events(struct perf_probe_event *pevs, int npevs);
int show_bootconfig_events(struct perf_probe_event *pevs, int npevs);
void cleanup_perf_probe_events(struct perf_probe_event *pevs, int npevs);

struct strfilter;

int del_perf_probe_events(struct strfilter *filter);

int show_perf_probe_event(const char *group, const char *event,
			  struct perf_probe_event *pev,
			  const char *module, bool use_stdout);
int show_perf_probe_events(struct strfilter *filter);
int show_line_range(struct line_range *lr, const char *module,
		    struct nsinfo *nsi, bool user);
int show_available_vars(struct perf_probe_event *pevs, int npevs,
			struct strfilter *filter);
int show_available_funcs(const char *module, struct nsinfo *nsi,
			 struct strfilter *filter, bool user);
void arch__fix_tev_from_maps(struct perf_probe_event *pev,
			     struct probe_trace_event *tev, struct map *map,
			     struct symbol *sym);


int e_snprintf(char *str, size_t size, const char *format, ...) __printf(3, 4);


#define MAX_EVENT_INDEX	1024

int copy_to_probe_trace_arg(struct probe_trace_arg *tvar,
			    struct perf_probe_arg *pvar);

struct map *get_target_map(const char *target, struct nsinfo *nsi, bool user);

void arch__post_process_probe_trace_events(struct perf_probe_event *pev,
					   int ntevs);

#endif 
