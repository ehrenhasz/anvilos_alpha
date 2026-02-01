#ifndef PERF_UTIL_BPF_SKEL_SAMPLE_FILTER_H
#define PERF_UTIL_BPF_SKEL_SAMPLE_FILTER_H

#define MAX_FILTERS  64

 
enum perf_bpf_filter_op {
	PBF_OP_EQ,
	PBF_OP_NEQ,
	PBF_OP_GT,
	PBF_OP_GE,
	PBF_OP_LT,
	PBF_OP_LE,
	PBF_OP_AND,
	PBF_OP_GROUP_BEGIN,
	PBF_OP_GROUP_END,
};

 
struct perf_bpf_filter_entry {
	enum perf_bpf_filter_op op;
	__u32 part;  
	__u64 flags;  
	__u64 value;
};

#endif  