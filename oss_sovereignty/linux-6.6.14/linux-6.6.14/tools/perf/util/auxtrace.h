#ifndef __PERF_AUXTRACE_H
#define __PERF_AUXTRACE_H
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>  
#include <linux/list.h>
#include <linux/perf_event.h>
#include <linux/types.h>
#include <perf/cpumap.h>
#include <asm/bitsperlong.h>
#include <asm/barrier.h>
union perf_event;
struct perf_session;
struct evlist;
struct evsel;
struct perf_tool;
struct mmap;
struct perf_sample;
struct option;
struct record_opts;
struct perf_record_auxtrace_error;
struct perf_record_auxtrace_info;
struct events_stats;
struct perf_pmu;
enum auxtrace_error_type {
       PERF_AUXTRACE_ERROR_ITRACE  = 1,
       PERF_AUXTRACE_ERROR_MAX
};
#define PERF_AUXTRACE_RECORD_ALIGNMENT 8
enum auxtrace_type {
	PERF_AUXTRACE_UNKNOWN,
	PERF_AUXTRACE_INTEL_PT,
	PERF_AUXTRACE_INTEL_BTS,
	PERF_AUXTRACE_CS_ETM,
	PERF_AUXTRACE_ARM_SPE,
	PERF_AUXTRACE_S390_CPUMSF,
	PERF_AUXTRACE_HISI_PTT,
};
enum itrace_period_type {
	PERF_ITRACE_PERIOD_INSTRUCTIONS,
	PERF_ITRACE_PERIOD_TICKS,
	PERF_ITRACE_PERIOD_NANOSECS,
};
#define AUXTRACE_ERR_FLG_OVERFLOW	(1 << ('o' - 'a'))
#define AUXTRACE_ERR_FLG_DATA_LOST	(1 << ('l' - 'a'))
#define AUXTRACE_LOG_FLG_ALL_PERF_EVTS	(1 << ('a' - 'a'))
#define AUXTRACE_LOG_FLG_ON_ERROR	(1 << ('e' - 'a'))
#define AUXTRACE_LOG_FLG_USE_STDOUT	(1 << ('o' - 'a'))
struct itrace_synth_opts {
	bool			set;
	bool			default_no_sample;
	bool			inject;
	bool			instructions;
	bool			cycles;
	bool			branches;
	bool			transactions;
	bool			ptwrites;
	bool			pwr_events;
	bool			other_events;
	bool			intr_events;
	bool			errors;
	bool			dont_decode;
	bool			log;
	bool			calls;
	bool			returns;
	bool			callchain;
	bool			add_callchain;
	bool			thread_stack;
	bool			last_branch;
	bool			add_last_branch;
	bool			approx_ipc;
	bool			flc;
	bool			llc;
	bool			tlb;
	bool			remote_access;
	bool			mem;
	bool			timeless_decoding;
	bool			vm_time_correlation;
	bool			vm_tm_corr_dry_run;
	char			*vm_tm_corr_args;
	unsigned int		callchain_sz;
	unsigned int		last_branch_sz;
	unsigned long long	period;
	enum itrace_period_type	period_type;
	unsigned long		initial_skip;
	unsigned long		*cpu_bitmap;
	struct perf_time_interval *ptime_range;
	int			range_num;
	unsigned int		error_plus_flags;
	unsigned int		error_minus_flags;
	unsigned int		log_plus_flags;
	unsigned int		log_minus_flags;
	unsigned int		quick;
	unsigned int		log_on_error_size;
};
struct auxtrace_index_entry {
	u64			file_offset;
	u64			sz;
};
#define PERF_AUXTRACE_INDEX_ENTRY_COUNT 256
struct auxtrace_index {
	struct list_head	list;
	size_t			nr;
	struct auxtrace_index_entry entries[PERF_AUXTRACE_INDEX_ENTRY_COUNT];
};
struct auxtrace {
	int (*process_event)(struct perf_session *session,
			     union perf_event *event,
			     struct perf_sample *sample,
			     struct perf_tool *tool);
	int (*process_auxtrace_event)(struct perf_session *session,
				      union perf_event *event,
				      struct perf_tool *tool);
	int (*queue_data)(struct perf_session *session,
			  struct perf_sample *sample, union perf_event *event,
			  u64 data_offset);
	void (*dump_auxtrace_sample)(struct perf_session *session,
				     struct perf_sample *sample);
	int (*flush_events)(struct perf_session *session,
			    struct perf_tool *tool);
	void (*free_events)(struct perf_session *session);
	void (*free)(struct perf_session *session);
	bool (*evsel_is_auxtrace)(struct perf_session *session,
				  struct evsel *evsel);
};
struct auxtrace_buffer {
	struct list_head	list;
	size_t			size;
	pid_t			pid;
	pid_t			tid;
	struct perf_cpu		cpu;
	void			*data;
	off_t			data_offset;
	void			*mmap_addr;
	size_t			mmap_size;
	bool			data_needs_freeing;
	bool			consecutive;
	u64			offset;
	u64			reference;
	u64			buffer_nr;
	size_t			use_size;
	void			*use_data;
};
struct auxtrace_queue {
	struct list_head	head;
	pid_t			tid;
	int			cpu;
	bool			set;
	void			*priv;
};
struct auxtrace_queues {
	struct auxtrace_queue	*queue_array;
	unsigned int		nr_queues;
	bool			new_data;
	bool			populated;
	u64			next_buffer_nr;
};
struct auxtrace_heap_item {
	unsigned int		queue_nr;
	u64			ordinal;
};
struct auxtrace_heap {
	struct auxtrace_heap_item	*heap_array;
	unsigned int		heap_cnt;
	unsigned int		heap_sz;
};
struct auxtrace_mmap {
	void		*base;
	void		*userpg;
	size_t		mask;
	size_t		len;
	u64		prev;
	int		idx;
	pid_t		tid;
	int		cpu;
};
struct auxtrace_mmap_params {
	size_t		mask;
	off_t		offset;
	size_t		len;
	int		prot;
	int		idx;
	pid_t		tid;
	bool		mmap_needed;
	struct perf_cpu	cpu;
};
struct auxtrace_record {
	int (*recording_options)(struct auxtrace_record *itr,
				 struct evlist *evlist,
				 struct record_opts *opts);
	size_t (*info_priv_size)(struct auxtrace_record *itr,
				 struct evlist *evlist);
	int (*info_fill)(struct auxtrace_record *itr,
			 struct perf_session *session,
			 struct perf_record_auxtrace_info *auxtrace_info,
			 size_t priv_size);
	void (*free)(struct auxtrace_record *itr);
	int (*snapshot_start)(struct auxtrace_record *itr);
	int (*snapshot_finish)(struct auxtrace_record *itr);
	int (*find_snapshot)(struct auxtrace_record *itr, int idx,
			     struct auxtrace_mmap *mm, unsigned char *data,
			     u64 *head, u64 *old);
	int (*parse_snapshot_options)(struct auxtrace_record *itr,
				      struct record_opts *opts,
				      const char *str);
	u64 (*reference)(struct auxtrace_record *itr);
	int (*read_finish)(struct auxtrace_record *itr, int idx);
	unsigned int alignment;
	unsigned int default_aux_sample_size;
	struct perf_pmu *pmu;
	struct evlist *evlist;
};
struct addr_filter {
	struct list_head	list;
	bool			range;
	bool			start;
	const char		*action;
	const char		*sym_from;
	const char		*sym_to;
	int			sym_from_idx;
	int			sym_to_idx;
	u64			addr;
	u64			size;
	const char		*filename;
	char			*str;
};
struct addr_filters {
	struct list_head	head;
	int			cnt;
};
struct auxtrace_cache;
#ifdef HAVE_AUXTRACE_SUPPORT
u64 compat_auxtrace_mmap__read_head(struct auxtrace_mmap *mm);
int compat_auxtrace_mmap__write_tail(struct auxtrace_mmap *mm, u64 tail);
static inline u64 auxtrace_mmap__read_head(struct auxtrace_mmap *mm,
					   int kernel_is_64_bit __maybe_unused)
{
	struct perf_event_mmap_page *pc = mm->userpg;
	u64 head;
#if BITS_PER_LONG == 32
	if (kernel_is_64_bit)
		return compat_auxtrace_mmap__read_head(mm);
#endif
	head = READ_ONCE(pc->aux_head);
	smp_rmb();
	return head;
}
static inline int auxtrace_mmap__write_tail(struct auxtrace_mmap *mm, u64 tail,
					    int kernel_is_64_bit __maybe_unused)
{
	struct perf_event_mmap_page *pc = mm->userpg;
#if BITS_PER_LONG == 32
	if (kernel_is_64_bit)
		return compat_auxtrace_mmap__write_tail(mm, tail);
#endif
	smp_mb();
	WRITE_ONCE(pc->aux_tail, tail);
	return 0;
}
int auxtrace_mmap__mmap(struct auxtrace_mmap *mm,
			struct auxtrace_mmap_params *mp,
			void *userpg, int fd);
void auxtrace_mmap__munmap(struct auxtrace_mmap *mm);
void auxtrace_mmap_params__init(struct auxtrace_mmap_params *mp,
				off_t auxtrace_offset,
				unsigned int auxtrace_pages,
				bool auxtrace_overwrite);
void auxtrace_mmap_params__set_idx(struct auxtrace_mmap_params *mp,
				   struct evlist *evlist,
				   struct evsel *evsel, int idx);
typedef int (*process_auxtrace_t)(struct perf_tool *tool,
				  struct mmap *map,
				  union perf_event *event, void *data1,
				  size_t len1, void *data2, size_t len2);
int auxtrace_mmap__read(struct mmap *map, struct auxtrace_record *itr,
			struct perf_tool *tool, process_auxtrace_t fn);
int auxtrace_mmap__read_snapshot(struct mmap *map,
				 struct auxtrace_record *itr,
				 struct perf_tool *tool, process_auxtrace_t fn,
				 size_t snapshot_size);
int auxtrace_queues__init(struct auxtrace_queues *queues);
int auxtrace_queues__add_event(struct auxtrace_queues *queues,
			       struct perf_session *session,
			       union perf_event *event, off_t data_offset,
			       struct auxtrace_buffer **buffer_ptr);
struct auxtrace_queue *
auxtrace_queues__sample_queue(struct auxtrace_queues *queues,
			      struct perf_sample *sample,
			      struct perf_session *session);
int auxtrace_queues__add_sample(struct auxtrace_queues *queues,
				struct perf_session *session,
				struct perf_sample *sample, u64 data_offset,
				u64 reference);
void auxtrace_queues__free(struct auxtrace_queues *queues);
int auxtrace_queues__process_index(struct auxtrace_queues *queues,
				   struct perf_session *session);
int auxtrace_queue_data(struct perf_session *session, bool samples,
			bool events);
struct auxtrace_buffer *auxtrace_buffer__next(struct auxtrace_queue *queue,
					      struct auxtrace_buffer *buffer);
void *auxtrace_buffer__get_data_rw(struct auxtrace_buffer *buffer, int fd, bool rw);
static inline void *auxtrace_buffer__get_data(struct auxtrace_buffer *buffer, int fd)
{
	return auxtrace_buffer__get_data_rw(buffer, fd, false);
}
void auxtrace_buffer__put_data(struct auxtrace_buffer *buffer);
void auxtrace_buffer__drop_data(struct auxtrace_buffer *buffer);
void auxtrace_buffer__free(struct auxtrace_buffer *buffer);
int auxtrace_heap__add(struct auxtrace_heap *heap, unsigned int queue_nr,
		       u64 ordinal);
void auxtrace_heap__pop(struct auxtrace_heap *heap);
void auxtrace_heap__free(struct auxtrace_heap *heap);
struct auxtrace_cache_entry {
	struct hlist_node hash;
	u32 key;
};
struct auxtrace_cache *auxtrace_cache__new(unsigned int bits, size_t entry_size,
					   unsigned int limit_percent);
void auxtrace_cache__free(struct auxtrace_cache *auxtrace_cache);
void *auxtrace_cache__alloc_entry(struct auxtrace_cache *c);
void auxtrace_cache__free_entry(struct auxtrace_cache *c, void *entry);
int auxtrace_cache__add(struct auxtrace_cache *c, u32 key,
			struct auxtrace_cache_entry *entry);
void auxtrace_cache__remove(struct auxtrace_cache *c, u32 key);
void *auxtrace_cache__lookup(struct auxtrace_cache *c, u32 key);
struct auxtrace_record *auxtrace_record__init(struct evlist *evlist,
					      int *err);
int auxtrace_parse_snapshot_options(struct auxtrace_record *itr,
				    struct record_opts *opts,
				    const char *str);
int auxtrace_parse_sample_options(struct auxtrace_record *itr,
				  struct evlist *evlist,
				  struct record_opts *opts, const char *str);
void auxtrace_regroup_aux_output(struct evlist *evlist);
int auxtrace_record__options(struct auxtrace_record *itr,
			     struct evlist *evlist,
			     struct record_opts *opts);
size_t auxtrace_record__info_priv_size(struct auxtrace_record *itr,
				       struct evlist *evlist);
int auxtrace_record__info_fill(struct auxtrace_record *itr,
			       struct perf_session *session,
			       struct perf_record_auxtrace_info *auxtrace_info,
			       size_t priv_size);
void auxtrace_record__free(struct auxtrace_record *itr);
int auxtrace_record__snapshot_start(struct auxtrace_record *itr);
int auxtrace_record__snapshot_finish(struct auxtrace_record *itr, bool on_exit);
int auxtrace_record__find_snapshot(struct auxtrace_record *itr, int idx,
				   struct auxtrace_mmap *mm,
				   unsigned char *data, u64 *head, u64 *old);
u64 auxtrace_record__reference(struct auxtrace_record *itr);
int auxtrace_record__read_finish(struct auxtrace_record *itr, int idx);
int auxtrace_index__auxtrace_event(struct list_head *head, union perf_event *event,
				   off_t file_offset);
int auxtrace_index__write(int fd, struct list_head *head);
int auxtrace_index__process(int fd, u64 size, struct perf_session *session,
			    bool needs_swap);
void auxtrace_index__free(struct list_head *head);
void auxtrace_synth_guest_error(struct perf_record_auxtrace_error *auxtrace_error, int type,
				int code, int cpu, pid_t pid, pid_t tid, u64 ip,
				const char *msg, u64 timestamp,
				pid_t machine_pid, int vcpu);
void auxtrace_synth_error(struct perf_record_auxtrace_error *auxtrace_error, int type,
			  int code, int cpu, pid_t pid, pid_t tid, u64 ip,
			  const char *msg, u64 timestamp);
int perf_event__process_auxtrace_info(struct perf_session *session,
				      union perf_event *event);
s64 perf_event__process_auxtrace(struct perf_session *session,
				 union perf_event *event);
int perf_event__process_auxtrace_error(struct perf_session *session,
				       union perf_event *event);
int itrace_do_parse_synth_opts(struct itrace_synth_opts *synth_opts,
			       const char *str, int unset);
int itrace_parse_synth_opts(const struct option *opt, const char *str,
			    int unset);
void itrace_synth_opts__set_default(struct itrace_synth_opts *synth_opts,
				    bool no_sample);
size_t perf_event__fprintf_auxtrace_error(union perf_event *event, FILE *fp);
void perf_session__auxtrace_error_inc(struct perf_session *session,
				      union perf_event *event);
void events_stats__auxtrace_error_warn(const struct events_stats *stats);
void addr_filters__init(struct addr_filters *filts);
void addr_filters__exit(struct addr_filters *filts);
int addr_filters__parse_bare_filter(struct addr_filters *filts,
				    const char *filter);
int auxtrace_parse_filters(struct evlist *evlist);
int auxtrace__process_event(struct perf_session *session, union perf_event *event,
			    struct perf_sample *sample, struct perf_tool *tool);
void auxtrace__dump_auxtrace_sample(struct perf_session *session,
				    struct perf_sample *sample);
int auxtrace__flush_events(struct perf_session *session, struct perf_tool *tool);
void auxtrace__free_events(struct perf_session *session);
void auxtrace__free(struct perf_session *session);
bool auxtrace__evsel_is_auxtrace(struct perf_session *session,
				 struct evsel *evsel);
#define ITRACE_HELP \
"				i[period]:    		synthesize instructions events\n" \
"				y[period]:    		synthesize cycles events (same period as i)\n" \
"				b:	    		synthesize branches events (branch misses for Arm SPE)\n" \
"				c:	    		synthesize branches events (calls only)\n"	\
"				r:	    		synthesize branches events (returns only)\n" \
"				x:	    		synthesize transactions events\n"		\
"				w:	    		synthesize ptwrite events\n"		\
"				p:	    		synthesize power events\n"			\
"				o:			synthesize other events recorded due to the use\n" \
"							of aux-output (refer to perf record)\n"	\
"				I:			synthesize interrupt or similar (asynchronous) events\n" \
"							(e.g. Intel PT Event Trace)\n" \
"				e[flags]:		synthesize error events\n" \
"							each flag must be preceded by + or -\n" \
"							error flags are: o (overflow)\n" \
"									 l (data lost)\n" \
"				d[flags]:		create a debug log\n" \
"							each flag must be preceded by + or -\n" \
"							log flags are: a (all perf events)\n" \
"							               o (output to stdout)\n" \
"				f:	    		synthesize first level cache events\n" \
"				m:	    		synthesize last level cache events\n" \
"				t:	    		synthesize TLB events\n" \
"				a:	    		synthesize remote access events\n" \
"				g[len]:     		synthesize a call chain (use with i or x)\n" \
"				G[len]:			synthesize a call chain on existing event records\n" \
"				l[len]:     		synthesize last branch entries (use with i or x)\n" \
"				L[len]:			synthesize last branch entries on existing event records\n" \
"				sNUMBER:    		skip initial number of events\n"		\
"				q:			quicker (less detailed) decoding\n" \
"				A:			approximate IPC\n" \
"				Z:			prefer to ignore timestamps (so-called \"timeless\" decoding)\n" \
"				PERIOD[ns|us|ms|i|t]:   specify period to sample stream\n" \
"				concatenate multiple options. Default is iybxwpe or cewp\n"
static inline
void itrace_synth_opts__set_time_range(struct itrace_synth_opts *opts,
				       struct perf_time_interval *ptime_range,
				       int range_num)
{
	opts->ptime_range = ptime_range;
	opts->range_num = range_num;
}
static inline
void itrace_synth_opts__clear_time_range(struct itrace_synth_opts *opts)
{
	opts->ptime_range = NULL;
	opts->range_num = 0;
}
#else
#include "debug.h"
static inline struct auxtrace_record *
auxtrace_record__init(struct evlist *evlist __maybe_unused,
		      int *err)
{
	*err = 0;
	return NULL;
}
static inline
void auxtrace_record__free(struct auxtrace_record *itr __maybe_unused)
{
}
static inline
int auxtrace_record__options(struct auxtrace_record *itr __maybe_unused,
			     struct evlist *evlist __maybe_unused,
			     struct record_opts *opts __maybe_unused)
{
	return 0;
}
static inline
int perf_event__process_auxtrace_info(struct perf_session *session __maybe_unused,
				      union perf_event *event __maybe_unused)
{
	return 0;
}
static inline
s64 perf_event__process_auxtrace(struct perf_session *session __maybe_unused,
				 union perf_event *event __maybe_unused)
{
	return 0;
}
static inline
int perf_event__process_auxtrace_error(struct perf_session *session __maybe_unused,
				       union perf_event *event __maybe_unused)
{
	return 0;
}
static inline
void perf_session__auxtrace_error_inc(struct perf_session *session
				      __maybe_unused,
				      union perf_event *event
				      __maybe_unused)
{
}
static inline
void events_stats__auxtrace_error_warn(const struct events_stats *stats
				       __maybe_unused)
{
}
static inline
int itrace_do_parse_synth_opts(struct itrace_synth_opts *synth_opts __maybe_unused,
			       const char *str __maybe_unused, int unset __maybe_unused)
{
	pr_err("AUX area tracing not supported\n");
	return -EINVAL;
}
static inline
int itrace_parse_synth_opts(const struct option *opt __maybe_unused,
			    const char *str __maybe_unused,
			    int unset __maybe_unused)
{
	pr_err("AUX area tracing not supported\n");
	return -EINVAL;
}
static inline
int auxtrace_parse_snapshot_options(struct auxtrace_record *itr __maybe_unused,
				    struct record_opts *opts __maybe_unused,
				    const char *str)
{
	if (!str)
		return 0;
	pr_err("AUX area tracing not supported\n");
	return -EINVAL;
}
static inline
int auxtrace_parse_sample_options(struct auxtrace_record *itr __maybe_unused,
				  struct evlist *evlist __maybe_unused,
				  struct record_opts *opts __maybe_unused,
				  const char *str)
{
	if (!str)
		return 0;
	pr_err("AUX area tracing not supported\n");
	return -EINVAL;
}
static inline
void auxtrace_regroup_aux_output(struct evlist *evlist __maybe_unused)
{
}
static inline
int auxtrace__process_event(struct perf_session *session __maybe_unused,
			    union perf_event *event __maybe_unused,
			    struct perf_sample *sample __maybe_unused,
			    struct perf_tool *tool __maybe_unused)
{
	return 0;
}
static inline
void auxtrace__dump_auxtrace_sample(struct perf_session *session __maybe_unused,
				    struct perf_sample *sample __maybe_unused)
{
}
static inline
int auxtrace__flush_events(struct perf_session *session __maybe_unused,
			   struct perf_tool *tool __maybe_unused)
{
	return 0;
}
static inline
void auxtrace__free_events(struct perf_session *session __maybe_unused)
{
}
static inline
void auxtrace_cache__free(struct auxtrace_cache *auxtrace_cache __maybe_unused)
{
}
static inline
void auxtrace__free(struct perf_session *session __maybe_unused)
{
}
static inline
int auxtrace_index__write(int fd __maybe_unused,
			  struct list_head *head __maybe_unused)
{
	return -EINVAL;
}
static inline
int auxtrace_index__process(int fd __maybe_unused,
			    u64 size __maybe_unused,
			    struct perf_session *session __maybe_unused,
			    bool needs_swap __maybe_unused)
{
	return -EINVAL;
}
static inline
void auxtrace_index__free(struct list_head *head __maybe_unused)
{
}
static inline
bool auxtrace__evsel_is_auxtrace(struct perf_session *session __maybe_unused,
				 struct evsel *evsel __maybe_unused)
{
	return false;
}
static inline
int auxtrace_parse_filters(struct evlist *evlist __maybe_unused)
{
	return 0;
}
int auxtrace_mmap__mmap(struct auxtrace_mmap *mm,
			struct auxtrace_mmap_params *mp,
			void *userpg, int fd);
void auxtrace_mmap__munmap(struct auxtrace_mmap *mm);
void auxtrace_mmap_params__init(struct auxtrace_mmap_params *mp,
				off_t auxtrace_offset,
				unsigned int auxtrace_pages,
				bool auxtrace_overwrite);
void auxtrace_mmap_params__set_idx(struct auxtrace_mmap_params *mp,
				   struct evlist *evlist,
				   struct evsel *evsel, int idx);
#define ITRACE_HELP ""
static inline
void itrace_synth_opts__set_time_range(struct itrace_synth_opts *opts
				       __maybe_unused,
				       struct perf_time_interval *ptime_range
				       __maybe_unused,
				       int range_num __maybe_unused)
{
}
static inline
void itrace_synth_opts__clear_time_range(struct itrace_synth_opts *opts
					 __maybe_unused)
{
}
#endif
#endif
