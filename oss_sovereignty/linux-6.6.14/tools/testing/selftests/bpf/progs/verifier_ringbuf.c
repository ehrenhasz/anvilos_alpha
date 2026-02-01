
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 4096);
} map_ringbuf SEC(".maps");

SEC("socket")
__description("ringbuf: invalid reservation offset 1")
__failure __msg("R1 must have zero offset when passed to release func")
__failure_unpriv
__naked void ringbuf_invalid_reservation_offset_1(void)
{
	asm volatile ("					\
	 		\
	r1 = 0;						\
	*(u64*)(r10 - 8) = r1;				\
	r1 = %[map_ringbuf] ll;				\
	r2 = 8;						\
	r3 = 0;						\
	call %[bpf_ringbuf_reserve];			\
	 \
	r6 = r0;					\
	 \
	if r0 == 0 goto l0_%=;				\
	 		\
	*(u64*)(r10 - 8) = r6;				\
	 			\
	r7 = *(u64*)(r10 - 8);				\
	 	\
	r1 = 0;						\
	*(u64*)(r7 + 0) = r1;				\
	 	\
	r1 = r7;					\
	 \
	r1 += 0xcafe;					\
	r2 = 0;						\
	call %[bpf_ringbuf_submit];			\
l0_%=:	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_ringbuf_reserve),
	  __imm(bpf_ringbuf_submit),
	  __imm_addr(map_ringbuf)
	: __clobber_all);
}

SEC("socket")
__description("ringbuf: invalid reservation offset 2")
__failure __msg("R7 min value is outside of the allowed memory range")
__failure_unpriv
__naked void ringbuf_invalid_reservation_offset_2(void)
{
	asm volatile ("					\
	 		\
	r1 = 0;						\
	*(u64*)(r10 - 8) = r1;				\
	r1 = %[map_ringbuf] ll;				\
	r2 = 8;						\
	r3 = 0;						\
	call %[bpf_ringbuf_reserve];			\
	 \
	r6 = r0;					\
	 \
	if r0 == 0 goto l0_%=;				\
	 		\
	*(u64*)(r10 - 8) = r6;				\
	 			\
	r7 = *(u64*)(r10 - 8);				\
	 \
	r7 += 0xcafe;					\
	 	\
	r1 = 0;						\
	*(u64*)(r7 + 0) = r1;				\
	 	\
	r1 = r7;					\
	r2 = 0;						\
	call %[bpf_ringbuf_submit];			\
l0_%=:	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_ringbuf_reserve),
	  __imm(bpf_ringbuf_submit),
	  __imm_addr(map_ringbuf)
	: __clobber_all);
}

SEC("xdp")
__description("ringbuf: check passing rb mem to helpers")
__success __retval(0)
__naked void passing_rb_mem_to_helpers(void)
{
	asm volatile ("					\
	r6 = r1;					\
	 		\
	r1 = 0;						\
	*(u64*)(r10 - 8) = r1;				\
	r1 = %[map_ringbuf] ll;				\
	r2 = 8;						\
	r3 = 0;						\
	call %[bpf_ringbuf_reserve];			\
	r7 = r0;					\
	 \
	if r0 != 0 goto l0_%=;				\
	exit;						\
l0_%=:	 \
	r1 = r6;					\
	r2 = r0;					\
	r3 = 8;						\
	r4 = 0;						\
	call %[bpf_fib_lookup];				\
	 			\
	r1 = r7;					\
	r2 = 0;						\
	call %[bpf_ringbuf_submit];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_fib_lookup),
	  __imm(bpf_ringbuf_reserve),
	  __imm(bpf_ringbuf_submit),
	  __imm_addr(map_ringbuf)
	: __clobber_all);
}

char _license[] SEC("license") = "GPL";
