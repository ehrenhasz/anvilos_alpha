
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, long long);
	__type(value, long long);
} map_hash_8b SEC(".maps");

SEC("lwt_in")
__description("variable-offset ctx access")
__failure __msg("variable ctx access var_off=(0x0; 0x4)")
__naked void variable_offset_ctx_access(void)
{
	asm volatile ("					\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	 						\
	r1 += r2;					\
	 				\
	r0 = *(u32*)(r1 + 0);				\
	exit;						\
"	::: __clobber_all);
}

SEC("cgroup/skb")
__description("variable-offset stack read, priv vs unpriv")
__success __failure_unpriv
__msg_unpriv("R2 variable stack access prohibited for !root")
__retval(0)
__naked void stack_read_priv_vs_unpriv(void)
{
	asm volatile ("					\
	 		\
	r0 = 0;						\
	*(u64*)(r10 - 8) = r0;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 8;					\
	 						\
	r2 += r10;					\
	 		\
	r0 = *(u32*)(r2 + 0);				\
	r0 = 0;						\
	exit;						\
"	::: __clobber_all);
}

SEC("cgroup/skb")
__description("variable-offset stack read, uninitialized")
__success
__failure_unpriv __msg_unpriv("R2 variable stack access prohibited for !root")
__naked void variable_offset_stack_read_uninitialized(void)
{
	asm volatile ("					\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 8;					\
	 						\
	r2 += r10;					\
	 		\
	r0 = *(u32*)(r2 + 0);				\
	r0 = 0;						\
	exit;						\
"	::: __clobber_all);
}

SEC("socket")
__description("variable-offset stack write, priv vs unpriv")
__success
/* Check that the maximum stack depth is correctly maintained according to the
 * maximum possible variable offset.
 */
__log_level(4) __msg("stack depth 16")
__failure_unpriv
/* Variable stack access is rejected for unprivileged.
 */
__msg_unpriv("R2 variable stack access prohibited for !root")
__retval(0)
__naked void stack_write_priv_vs_unpriv(void)
{
	asm volatile ("                               \
	                     \
	r2 = *(u32*)(r1 + 0);                         \
	         \
	r2 &= 8;                                      \
	r2 -= 16;                                     \
	                                            \
	r2 += r10;                                    \
	         \
	r0 = 0;                                       \
	*(u64*)(r2 + 0) = r0;                         \
	exit;                                         \
"	::: __clobber_all);
}

/* Similar to the previous test, but this time also perform a read from the
 * address written to with a variable offset. The read is allowed, showing that,
 * after a variable-offset write, a priviledged program can read the slots that
 * were in the range of that write (even if the verifier doesn't actually know if
 * the slot being read was really written to or not.
 *
 * Despite this test being mostly a superset, the previous test is also kept for
 * the sake of it checking the stack depth in the case where there is no read.
 */
SEC("socket")
__description("variable-offset stack write followed by read")
__success
/* Check that the maximum stack depth is correctly maintained according to the
 * maximum possible variable offset.
 */
__log_level(4) __msg("stack depth 16")
__failure_unpriv
__msg_unpriv("R2 variable stack access prohibited for !root")
__retval(0)
__naked void stack_write_followed_by_read(void)
{
	asm volatile ("					\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 8;					\
	r2 -= 16;					\
	 						\
	r2 += r10;					\
	 		\
	r0 = 0;						\
	*(u64*)(r2 + 0) = r0;				\
	  \
	r3 = *(u64*)(r2 + 0);				\
	r0 = 0;						\
	exit;						\
"	::: __clobber_all);
}

SEC("socket")
__description("variable-offset stack write clobbers spilled regs")
__failure
/* In the priviledged case, dereferencing a spilled-and-then-filled
 * register is rejected because the previous variable offset stack
 * write might have overwritten the spilled pointer (i.e. we lose track
 * of the spilled register when we analyze the write).
 */
__msg("R2 invalid mem access 'scalar'")
__failure_unpriv
/* The unprivileged case is not too interesting; variable
 * stack access is rejected.
 */
__msg_unpriv("R2 variable stack access prohibited for !root")
__naked void stack_write_clobbers_spilled_regs(void)
{
	asm volatile ("					\
	 						\
	r6 = 0;						\
	 				\
	r0 = %[map_hash_8b] ll;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 8;					\
	r2 -= 16;					\
	 						\
	r2 += r10;					\
	 		\
	*(u64*)(r10 - 8) = r0;				\
	 \
	r0 = 0;						\
	*(u64*)(r2 + 0) = r0;				\
	 		\
	r2 = *(u64*)(r10 - 8);				\
	 	\
	r0 = *(u64*)(r2 + 8);				\
	exit;						\
"	:
	: __imm_addr(map_hash_8b)
	: __clobber_all);
}

SEC("sockops")
__description("indirect variable-offset stack access, unbounded")
__failure __msg("invalid unbounded variable-offset indirect access to stack R4")
__naked void variable_offset_stack_access_unbounded(void)
{
	asm volatile ("					\
	r2 = 6;						\
	r3 = 28;					\
	 	\
	r4 = 0;						\
	*(u64*)(r10 - 16) = r4;				\
	r4 = 0;						\
	*(u64*)(r10 - 8) = r4;				\
	 			\
	r4 = *(u64*)(r1 + %[bpf_sock_ops_bytes_received]);\
	 \
	if r4 s< 0 goto l0_%=;				\
	 						\
	r4 -= 16;					\
	r4 += r10;					\
	r5 = 8;						\
	 		\
	call %[bpf_getsockopt];				\
l0_%=:	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_getsockopt),
	  __imm_const(bpf_sock_ops_bytes_received, offsetof(struct bpf_sock_ops, bytes_received))
	: __clobber_all);
}

SEC("lwt_in")
__description("indirect variable-offset stack access, max out of bound")
__failure __msg("invalid variable-offset indirect access to stack R2")
__naked void access_max_out_of_bound(void)
{
	asm volatile ("					\
	 		\
	r2 = 0;						\
	*(u64*)(r10 - 8) = r2;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 8;					\
	 						\
	r2 += r10;					\
	 			\
	r1 = %[map_hash_8b] ll;				\
	call %[bpf_map_lookup_elem];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

SEC("lwt_in")
__description("indirect variable-offset stack access, min out of bound")
__failure __msg("invalid variable-offset indirect access to stack R2")
__naked void access_min_out_of_bound(void)
{
	asm volatile ("					\
	 		\
	r2 = 0;						\
	*(u64*)(r10 - 8) = r2;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 516;					\
	 						\
	r2 += r10;					\
	 			\
	r1 = %[map_hash_8b] ll;				\
	call %[bpf_map_lookup_elem];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

SEC("cgroup/skb")
__description("indirect variable-offset stack access, min_off < min_initialized")
__success
__failure_unpriv __msg_unpriv("R2 variable stack access prohibited for !root")
__naked void access_min_off_min_initialized(void)
{
	asm volatile ("					\
	 	\
	r2 = 0;						\
	*(u64*)(r10 - 8) = r2;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 16;					\
	 						\
	r2 += r10;					\
	 		\
	r1 = %[map_hash_8b] ll;				\
	call %[bpf_map_lookup_elem];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

SEC("cgroup/skb")
__description("indirect variable-offset stack access, priv vs unpriv")
__success __failure_unpriv
__msg_unpriv("R2 variable stack access prohibited for !root")
__retval(0)
__naked void stack_access_priv_vs_unpriv(void)
{
	asm volatile ("					\
	 	\
	r2 = 0;						\
	*(u64*)(r10 - 16) = r2;				\
	r2 = 0;						\
	*(u64*)(r10 - 8) = r2;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 16;					\
	 						\
	r2 += r10;					\
	 		\
	r1 = %[map_hash_8b] ll;				\
	call %[bpf_map_lookup_elem];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

SEC("lwt_in")
__description("indirect variable-offset stack access, ok")
__success __retval(0)
__naked void variable_offset_stack_access_ok(void)
{
	asm volatile ("					\
	 	\
	r2 = 0;						\
	*(u64*)(r10 - 16) = r2;				\
	r2 = 0;						\
	*(u64*)(r10 - 8) = r2;				\
	 			\
	r2 = *(u32*)(r1 + 0);				\
	 		\
	r2 &= 4;					\
	r2 -= 16;					\
	 						\
	r2 += r10;					\
	 		\
	r1 = %[map_hash_8b] ll;				\
	call %[bpf_map_lookup_elem];			\
	r0 = 0;						\
	exit;						\
"	:
	: __imm(bpf_map_lookup_elem),
	  __imm_addr(map_hash_8b)
	: __clobber_all);
}

char _license[] SEC("license") = "GPL";
