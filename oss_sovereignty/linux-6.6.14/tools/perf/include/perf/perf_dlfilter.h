 
 
#ifndef _LINUX_PERF_DLFILTER_H
#define _LINUX_PERF_DLFILTER_H

#include <linux/perf_event.h>
#include <linux/types.h>

 
#define PERF_DLFILTER_HAS_MACHINE_PID

 
enum {
	PERF_DLFILTER_FLAG_BRANCH	= 1ULL << 0,
	PERF_DLFILTER_FLAG_CALL		= 1ULL << 1,
	PERF_DLFILTER_FLAG_RETURN	= 1ULL << 2,
	PERF_DLFILTER_FLAG_CONDITIONAL	= 1ULL << 3,
	PERF_DLFILTER_FLAG_SYSCALLRET	= 1ULL << 4,
	PERF_DLFILTER_FLAG_ASYNC	= 1ULL << 5,
	PERF_DLFILTER_FLAG_INTERRUPT	= 1ULL << 6,
	PERF_DLFILTER_FLAG_TX_ABORT	= 1ULL << 7,
	PERF_DLFILTER_FLAG_TRACE_BEGIN	= 1ULL << 8,
	PERF_DLFILTER_FLAG_TRACE_END	= 1ULL << 9,
	PERF_DLFILTER_FLAG_IN_TX	= 1ULL << 10,
	PERF_DLFILTER_FLAG_VMENTRY	= 1ULL << 11,
	PERF_DLFILTER_FLAG_VMEXIT	= 1ULL << 12,
};

 
struct perf_dlfilter_sample {
	__u32 size;  
	__u16 ins_lat;		 
	__u16 p_stage_cyc;	 
	__u64 ip;
	__s32 pid;
	__s32 tid;
	__u64 time;
	__u64 addr;
	__u64 id;
	__u64 stream_id;
	__u64 period;
	__u64 weight;		 
	__u64 transaction;	 
	__u64 insn_cnt;	 
	__u64 cyc_cnt;		 
	__s32 cpu;
	__u32 flags;		 
	__u64 data_src;		 
	__u64 phys_addr;	 
	__u64 data_page_size;	 
	__u64 code_page_size;	 
	__u64 cgroup;		 
	__u8  cpumode;		 
	__u8  addr_correlates_sym;  
	__u16 misc;		 
	__u32 raw_size;		 
	const void *raw_data;	 
	__u64 brstack_nr;	 
	const struct perf_branch_entry *brstack;  
	__u64 raw_callchain_nr;	 
	const __u64 *raw_callchain;  
	const char *event;
	__s32 machine_pid;
	__s32 vcpu;
};

 
struct perf_dlfilter_al {
	__u32 size;  
	__u32 symoff;
	const char *sym;
	__u64 addr;  
	__u64 sym_start;
	__u64 sym_end;
	const char *dso;
	__u8  sym_binding;  
	__u8  is_64_bit;  
	__u8  is_kernel_ip;  
	__u32 buildid_size;
	__u8 *buildid;
	 
	__u8 filtered;  
	const char *comm;
	void *priv;  
};

struct perf_dlfilter_fns {
	 
	const struct perf_dlfilter_al *(*resolve_ip)(void *ctx);
	 
	const struct perf_dlfilter_al *(*resolve_addr)(void *ctx);
	 
	char **(*args)(void *ctx, int *dlargc);
	 
	__s32 (*resolve_address)(void *ctx, __u64 address, struct perf_dlfilter_al *al);
	 
	const __u8 *(*insn)(void *ctx, __u32 *length);
	 
	const char *(*srcline)(void *ctx, __u32 *line_number);
	 
	struct perf_event_attr *(*attr)(void *ctx);
	 
	__s32 (*object_code)(void *ctx, __u64 ip, void *buf, __u32 len);
	 
	void (*al_cleanup)(void *ctx, struct perf_dlfilter_al *al);
	 
	void *(*reserved[119])(void *);
};

 
int start(void **data, void *ctx);

 
int stop(void *data, void *ctx);

 
int filter_event(void *data, const struct perf_dlfilter_sample *sample, void *ctx);

 
int filter_event_early(void *data, const struct perf_dlfilter_sample *sample, void *ctx);

 
const char *filter_description(const char **long_description);

#endif
