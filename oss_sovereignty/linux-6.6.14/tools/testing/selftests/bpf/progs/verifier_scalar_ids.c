

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

 
SEC("socket")
__success __log_level(2)
__msg("frame0: regs=r0,r1,r2 stack= before 4: (bf) r3 = r10")
__msg("frame0: regs=r0,r1,r2 stack= before 3: (bf) r2 = r0")
__msg("frame0: regs=r0,r1 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__msg("frame0: regs=r0 stack= before 0: (85) call bpf_ktime_get_ns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_same_state(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"r2 = r0;"
	 
	"r3 = r10;"
	"r3 += r0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("frame0: last_idx 6 first_idx 5 subseq_idx -1")
__msg("frame0: regs=r0,r1,r2 stack= before 5: (bf) r3 = r10")
__msg("frame0: parent state regs=r0,r1,r2 stack=:")
__msg("frame0: regs=r0,r1,r2 stack= before 4: (05) goto pc+0")
__msg("frame0: regs=r0,r1,r2 stack= before 3: (bf) r2 = r0")
__msg("frame0: regs=r0,r1 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__msg("frame0: parent state regs=r0 stack=:")
__msg("frame0: regs=r0 stack= before 0: (85) call bpf_ktime_get_ns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_cross_state(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"r2 = r0;"
	 
	"goto +0;"
	 
	"r3 = r10;"
	"r3 += r0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("frame0: regs=r0,r2 stack= before 5: (bf) r3 = r10")
__msg("frame0: regs=r0,r2 stack= before 4: (b7) r1 = 0")
__msg("frame0: regs=r0,r2 stack= before 3: (bf) r2 = r0")
__msg("frame0: regs=r0 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__msg("frame0: regs=r0 stack= before 0: (85) call bpf_ktime_get_ns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_same_state_broken_link(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"r2 = r0;"
	 
	"r1 = 0;"
	 
	"r3 = r10;"
	"r3 += r0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("frame0: regs=r0,r2 stack= before 6: (bf) r3 = r10")
__msg("frame0: regs=r0,r2 stack= before 5: (b7) r1 = 0")
__msg("frame0: parent state regs=r0,r2 stack=:")
__msg("frame0: regs=r0,r1,r2 stack= before 4: (05) goto pc+0")
__msg("frame0: regs=r0,r1,r2 stack= before 3: (bf) r2 = r0")
__msg("frame0: regs=r0,r1 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__msg("frame0: parent state regs=r0 stack=:")
__msg("frame0: regs=r0 stack= before 0: (85) call bpf_ktime_get_ns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_cross_state_broken_link(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"r2 = r0;"
	 
	"goto +0;"
	 
	"r1 = 0;"
	 
	"r3 = r10;"
	"r3 += r0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("11: (0f) r2 += r1")
 
__msg("frame2: last_idx 11 first_idx 10 subseq_idx -1")
__msg("frame2: regs=r1 stack= before 10: (bf) r2 = r10")
__msg("frame2: parent state regs=r1 stack=")
 
__msg("frame1: parent state regs=r6,r7 stack=")
__msg("frame0: parent state regs=r6 stack=")
 
__msg("frame2: last_idx 8 first_idx 8 subseq_idx 10")
__msg("frame2: regs=r1 stack= before 8: (85) call pc+1")
 
__msg("frame1: parent state regs=r1,r6,r7 stack=")
__msg("frame0: parent state regs=r6 stack=")
 
__msg("frame1: last_idx 7 first_idx 6 subseq_idx 8")
__msg("frame1: regs=r1,r6,r7 stack= before 7: (bf) r7 = r1")
__msg("frame1: regs=r1,r6 stack= before 6: (bf) r6 = r1")
__msg("frame1: parent state regs=r1 stack=")
__msg("frame0: parent state regs=r6 stack=")
 
__msg("frame1: last_idx 4 first_idx 4 subseq_idx 6")
__msg("frame1: regs=r1 stack= before 4: (85) call pc+1")
__msg("frame0: parent state regs=r1,r6 stack=")
 
__msg("frame0: last_idx 3 first_idx 1 subseq_idx 4")
__msg("frame0: regs=r0,r1,r6 stack= before 3: (bf) r6 = r0")
__msg("frame0: regs=r0,r1 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_many_frames(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"r6 = r0;"
	"call precision_many_frames__foo;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

static __naked __noinline __used
void precision_many_frames__foo(void)
{
	asm volatile (
	 
	"r6 = r1;"
	"r7 = r1;"
	"call precision_many_frames__bar;"
	"exit"
	::: __clobber_all);
}

static __naked __noinline __used
void precision_many_frames__bar(void)
{
	asm volatile (
	 
	"r2 = r10;"
	"r2 += r1;"
	"r0 = 0;"
	"exit;"
	::: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
 
__msg("frame1: regs=r1 stack=-8,-16 before 9: (bf) r2 = r10")
__msg("frame1: regs=r1 stack=-8,-16 before 8: (7b) *(u64 *)(r10 -16) = r1")
__msg("frame1: regs=r1 stack=-8 before 7: (7b) *(u64 *)(r10 -8) = r1")
__msg("frame1: regs=r1 stack= before 4: (85) call pc+2")
 
__msg("frame0: regs=r0,r1 stack=-8 before 3: (7b) *(u64 *)(r10 -8) = r1")
__msg("frame0: regs=r0,r1 stack= before 2: (bf) r1 = r0")
__msg("frame0: regs=r0 stack= before 1: (57) r0 &= 255")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_stack(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	"*(u64*)(r10 - 8) = r1;"
	"call precision_stack__foo;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

static __naked __noinline __used
void precision_stack__foo(void)
{
	asm volatile (
	 
	"*(u64*)(r10 - 8) = r1;"
	"*(u64*)(r10 - 16) = r1;"
	 
	"r2 = r10;"
	"r2 += r1;"
	"exit"
	::: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
 
__msg("11: (0f) r3 += r7")
__msg("frame0: regs=r6,r7 stack= before 10: (bf) r3 = r10")
 
__msg("frame0: regs=r6,r7 stack= before 3: (bf) r7 = r0")
__msg("frame0: regs=r0,r6 stack= before 2: (bf) r6 = r0")
 
__msg("12: (0f) r3 += r9")
__msg("frame0: regs=r8,r9 stack= before 11: (0f) r3 += r7")
 
__msg("frame0: regs=r8,r9 stack= before 7: (bf) r9 = r0")
__msg("frame0: regs=r0,r8 stack= before 6: (bf) r8 = r0")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void precision_two_ids(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	"r6 = r0;"
	"r7 = r0;"
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	"r8 = r0;"
	"r9 = r0;"
	 
	"r0 = 0;"
	 
	"goto +0;"
	"r3 = r10;"
	 
	"r3 += r7;"
	 
	"r3 += r9;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__failure __msg("register with unbounded min value")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void check_ids_in_regsafe(void)
{
	asm volatile (
	 
	"r1 = 0;"
	"*(u64*)(r10 - 8) = r1;"
	 
	"r9 = r10;"
	"r9 += -8;"
	 
	"call %[bpf_ktime_get_ns];"
	"r7 = r0;"
	 
	"call %[bpf_ktime_get_ns];"
	"r6 = r0;"
	 
	"if r6 > r7 goto l1_%=;"
	"r7 = r6;"
"l1_%=:"
	 
	"if r7 > 4 goto l2_%=;"
	 
	"r9 += r6;"
	"r0 = *(u8*)(r9 + 0);"
"l2_%=:"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__failure __msg("register with unbounded min value")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void check_ids_in_regsafe_2(void)
{
	asm volatile (
	 
	"r1 = 0;"
	"*(u64*)(r10 - 8) = r1;"
	 
	"r9 = r10;"
	"r9 += -8;"
	 
	"call %[bpf_ktime_get_ns];"
	"r8 = r0;"
	 
	"call %[bpf_ktime_get_ns];"
	"r7 = r0;"
	 
	"call %[bpf_ktime_get_ns];"
	"r6 = r0;"
	 
	"r0 = 0;"
	 
	"if r6 > r7 goto l1_%=;"
	 
	"r6 = r7;"
"l0_%=:"
	 
	"if r7 > 4 goto l2_%=;"
	 
	"r9 += r6;"
	"r0 = *(u8*)(r9 + 0);"
"l2_%=:"
	"r0 = 0;"
	"exit;"
"l1_%=:"
	 
	"r6 = r8;"
	"goto l0_%=;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("11: (1d) if r3 == r4 goto pc+0")
__msg("frame 0: propagating r3,r4")
__msg("11: safe")
__msg("processed 15 insns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void no_scalar_id_for_const(void)
{
	asm volatile (
	"call %[bpf_ktime_get_ns];"
	 
	"if r0 > 7 goto l0_%=;"
	 
	"r1 = 0;"
	"r1 = r1;"
	"r3 = r1;"
	"r4 = r1;"
	"goto l1_%=;"
"l0_%=:"
	 
	"r1 = 0;"
	"r2 = 0;"
	"r3 = r1;"
	"r4 = r2;"
"l1_%=:"
	 
	"if r3 == r4 goto +0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("11: (1e) if w3 == w4 goto pc+0")
__msg("frame 0: propagating r3,r4")
__msg("11: safe")
__msg("processed 15 insns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void no_scalar_id_for_const32(void)
{
	asm volatile (
	"call %[bpf_ktime_get_ns];"
	 
	"if r0 > 7 goto l0_%=;"
	 
	"w1 = 0;"
	"w1 = w1;"
	"w3 = w1;"
	"w4 = w1;"
	"goto l1_%=;"
"l0_%=:"
	 
	"w1 = 0;"
	"w2 = 0;"
	"w3 = w1;"
	"w4 = w2;"
"l1_%=:"
	 
	"if w3 == w4 goto +0;"
	"r0 = 0;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("6: (25) if r6 > 0x7 goto pc+1")
__msg("7: (57) r1 &= 255")
__msg("8: (bf) r2 = r10")
__msg("from 6 to 8: safe")
__msg("processed 12 insns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void ignore_unique_scalar_ids_cur(void)
{
	asm volatile (
	"call %[bpf_ktime_get_ns];"
	"r6 = r0;"
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	 
	"r0 = 0;"
	"if r6 > 7 goto l0_%=;"
	 
	"r1 &= 0xff;"
"l0_%=:"
	 
	"r2 = r10;"
	"r2 += r1;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
__msg("6: (25) if r6 > 0x7 goto pc+1")
__msg("7: (05) goto pc+1")
__msg("9: (bf) r2 = r10")
__msg("9: safe")
__msg("processed 13 insns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void ignore_unique_scalar_ids_old(void)
{
	asm volatile (
	"call %[bpf_ktime_get_ns];"
	"r6 = r0;"
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	 
	"r1 = r0;"
	 
	"r0 = 0;"
	"if r6 > 7 goto l1_%=;"
	"goto l0_%=;"
"l1_%=:"
	 
	"r1 &= 0xff;"
"l0_%=:"
	 
	"r2 = r10;"
	"r2 += r1;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

 
SEC("socket")
__success __log_level(2)
 
__msg("13: (95) exit")
__msg("13: (95) exit")
__msg("processed 18 insns")
__flag(BPF_F_TEST_STATE_FREQ)
__naked void two_old_ids_one_cur_id(void)
{
	asm volatile (
	 
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	"r6 = r0;"
	"call %[bpf_ktime_get_ns];"
	"r0 &= 0xff;"
	"r7 = r0;"
	"r0 = 0;"
	 
	"if r6 > r7 goto l0_%=;"
	"goto l1_%=;"
"l0_%=:"
	"r6 = r7;"
"l1_%=:"
	 
	"r2 = r10;"
	"r2 += r6;"
	"r2 += r7;"
	"exit;"
	:
	: __imm(bpf_ktime_get_ns)
	: __clobber_all);
}

char _license[] SEC("license") = "GPL";
