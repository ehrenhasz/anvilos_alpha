
 

#include <errno.h>
#include <string.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

char _license[] SEC("license") = "GPL";

#define ITER_HELPERS						\
	  __imm(bpf_iter_num_new),				\
	  __imm(bpf_iter_num_next),				\
	  __imm(bpf_iter_num_destroy)

SEC("?raw_tp")
__success
int force_clang_to_emit_btf_for_externs(void *ctx)
{
	 
	bpf_repeat(0);

	return 0;
}

SEC("?raw_tp")
__success __log_level(2)
__msg("fp-8_w=iter_num(ref_id=1,state=active,depth=0)")
int create_and_destroy(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("Unreleased reference id=1")
int create_and_forget_to_destroy_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int destroy_without_creating_fail(void *ctx)
{
	 
	struct bpf_iter_num iter;

	asm volatile (
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int compromise_iter_w_direct_write_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"

		 
		"*(u64 *)(%[iter] + 0) = r0;"

		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("Unreleased reference id=1")
int compromise_iter_w_direct_write_and_skip_destroy_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"

		 
		"*(u64 *)(%[iter] + 0) = r0;"

		 
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int compromise_iter_w_helper_write_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"

		 
		"r1 = %[iter];"
		"r1 += 7;"
		"r2 = 1;"
		"r3 = 0;"  
		"call %[bpf_probe_read_kernel];"

		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS, __imm(bpf_probe_read_kernel)
		: __clobber_common
	);

	return 0;
}

static __noinline void subprog_with_iter(void)
{
	struct bpf_iter_num iter;

	bpf_iter_num_new(&iter, 0, 1);

	return;
}

SEC("?raw_tp")
__failure
 
__msg("returning from callee:")
__msg("Unreleased reference id=1")
int leak_iter_from_subprog_fail(void *ctx)
{
	subprog_with_iter();

	return 0;
}

SEC("?raw_tp")
__success __log_level(2)
__msg("fp-8_w=iter_num(ref_id=1,state=active,depth=0)")
int valid_stack_reuse(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"

		 

		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected uninitialized iter_num as arg #1")
int double_create_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int double_destroy_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int next_without_new_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_next];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("expected an initialized iter_num as arg #1")
int next_after_destroy_fail(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_destroy];"
		 
		"r1 = %[iter];"
		"call %[bpf_iter_num_next];"
		:
		: __imm_ptr(iter), ITER_HELPERS
		: __clobber_common
	);

	return 0;
}

SEC("?raw_tp")
__failure __msg("invalid read from stack")
int __naked read_from_iter_slot_fail(void)
{
	asm volatile (
		 
		"r6 = r10;"
		"r6 += -24;"

		 
		"r1 = r6;"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"

		 
		"r7 = *(u64 *)(r6 + 0);"
		"r8 = *(u64 *)(r6 + 8);"

		 
		"r1 = r6;"
		"call %[bpf_iter_num_destroy];"

		 
		"r0 = r7;"
		"if r7 > r8 goto +1;"
		"r0 = r8;"
		"exit;"
		:
		: ITER_HELPERS
		: __clobber_common, "r6", "r7", "r8"
	);
}

int zero;

SEC("?raw_tp")
__failure
__flag(BPF_F_TEST_STATE_FREQ)
__msg("Unreleased reference")
int stacksafe_should_not_conflate_stack_spill_and_iter(void *ctx)
{
	struct bpf_iter_num iter;

	asm volatile (
		 
		"call %[bpf_get_prandom_u32];"
		"r6 = r0;"
		"call %[bpf_get_prandom_u32];"
		"r7 = r0;"

		"if r6 > r7 goto bad;"  

		 
		"*(u64 *)(%[iter] + 0) = r6;"

		"goto skip_bad;"

	"bad:"
		 
		"r1 = %[iter];"
		"r2 = 0;"
		"r3 = 1000;"
		"call %[bpf_iter_num_new];"

		 
		"*(u64 *)(%[iter] + 0) = r6;"

	"skip_bad:"
		"goto +0;"  

		 
		"*(u64 *)(%[iter] + 0) = r6;"
		:
		: __imm_ptr(iter),
		  __imm_addr(zero),
		  __imm(bpf_get_prandom_u32),
		  __imm(bpf_dynptr_from_mem),
		  ITER_HELPERS
		: __clobber_common, "r6", "r7"
	);

	return 0;
}
