 

#ifndef __PERF_BPF_UTILS_H
#define __PERF_BPF_UTILS_H

#define ptr_to_u64(ptr)    ((__u64)(unsigned long)(ptr))

#ifdef HAVE_LIBBPF_SUPPORT

#include <bpf/libbpf.h>

 
enum perf_bpil_array_types {
	PERF_BPIL_FIRST_ARRAY = 0,
	PERF_BPIL_JITED_INSNS = 0,
	PERF_BPIL_XLATED_INSNS,
	PERF_BPIL_MAP_IDS,
	PERF_BPIL_JITED_KSYMS,
	PERF_BPIL_JITED_FUNC_LENS,
	PERF_BPIL_FUNC_INFO,
	PERF_BPIL_LINE_INFO,
	PERF_BPIL_JITED_LINE_INFO,
	PERF_BPIL_PROG_TAGS,
	PERF_BPIL_LAST_ARRAY,
};

struct perf_bpil {
	 
	__u32			info_len;
	 
	__u32			data_len;
	 
	__u64			arrays;
	struct bpf_prog_info	info;
	__u8			data[];
};

struct perf_bpil *
get_bpf_prog_info_linear(int fd, __u64 arrays);

void
bpil_addr_to_offs(struct perf_bpil *info_linear);

void
bpil_offs_to_addr(struct perf_bpil *info_linear);

#endif  
#endif  
