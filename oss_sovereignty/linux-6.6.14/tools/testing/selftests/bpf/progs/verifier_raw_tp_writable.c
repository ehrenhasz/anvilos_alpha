
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, long long);
	__type(value, long long);
} map_hash_8b SEC(".maps");

SEC("raw_tracepoint.w")
__description("raw_tracepoint_writable: reject variable offset")
__failure
__msg("R6 invalid variable buffer offset: off=0, var_off=(0x0; 0xffffffff)")
__flag(BPF_F_ANY_ALIGNMENT)
__naked void tracepoint_writable_reject_variable_offset(void)
{
	asm volatile ("					\
	 			\
	r6 = *(u64*)(r1 + 0);				\
	r1 = %[map_hash_8b] ll;				\
	 		\
	w0 = 0;						\
	r2 = r10;					\
	r2 += -8;					\
	*(u64*)(r2 + 0) = r0;				\
	 				\
	call %[bpf_map_lookup_elem];			\
	 			\
	if r0 != 0 goto l0_%=;				\
	exit;						\
l0_%=:	 \
	r0 = *(u32*)(r0 + 0);				\
	r6 += r0;					\
	 			\
	r7 = 4242;					\
	*(u64*)(r6 + 0) = r7;				\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

char _license[] SEC("license") = "GPL";
