

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

 
SEC("socket")
__naked int read_uninit_stack_fixed_off(void *ctx)
{
	asm volatile ("					\
		r0 = 0;					\
		 	\
		*(u64*)(r10 - 128) = r1;		\
		r1 = *(u8 *)(r10 - 8 );			\
		r0 += r1;				\
		r1 = *(u8 *)(r10 - 11);			\
		r1 = *(u8 *)(r10 - 13);			\
		r1 = *(u8 *)(r10 - 15);			\
		r1 = *(u16*)(r10 - 16);			\
		r1 = *(u32*)(r10 - 32);			\
		r1 = *(u64*)(r10 - 64);			\
		 					\
		*(u32*)(r10 - 72) = r1;			\
		r1 = *(u64*)(r10 - 72);			\
		r0 = 0;					\
		exit;					\
"
		      ::: __clobber_all);
}

/* Read an uninitialized value from stack at a variable offset */
SEC("socket")
__naked int read_uninit_stack_var_off(void *ctx)
{
	asm volatile ("					\
		call %[bpf_get_prandom_u32];		\
		 	\
		*(u64*)(r10 - 64) = r0;			\
		r0 = -r0;				\
		 		\
		if r0 s<= -32 goto exit_%=;		\
		if r0 s>= 0 goto exit_%=;		\
		 		\
		r1 = r10;				\
		r1 += r0;				\
		r2 = *(u8*)(r1 + 0);			\
exit_%=:	r0 = 0;					\
		exit;					\
"
		      :
		      : __imm(bpf_get_prandom_u32)
		      : __clobber_all);
}

static __noinline void dummy(void) {}

/* Pass a pointer to uninitialized stack memory to a helper.
 * Passed memory block should be marked as STACK_MISC after helper call.
 */
SEC("socket")
__log_level(7) __msg("fp-104=mmmmmmmm")
__naked int helper_uninit_to_misc(void *ctx)
{
	asm volatile ("					\
		 	\
		*(u64*)(r10 - 128) = r1;		\
		r1 = r10;				\
		r1 += -128;				\
		r2 = 32;				\
		call %[bpf_trace_printk];		\
		 					\
		call %[dummy];				\
		r0 = 0;					\
		exit;					\
"
		      :
		      : __imm(bpf_trace_printk),
			__imm(dummy)
		      : __clobber_all);
}

char _license[] SEC("license") = "GPL";
