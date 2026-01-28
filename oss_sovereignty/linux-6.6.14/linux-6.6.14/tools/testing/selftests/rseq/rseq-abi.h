#ifndef _RSEQ_ABI_H
#define _RSEQ_ABI_H
#include <linux/types.h>
#include <asm/byteorder.h>
enum rseq_abi_cpu_id_state {
	RSEQ_ABI_CPU_ID_UNINITIALIZED			= -1,
	RSEQ_ABI_CPU_ID_REGISTRATION_FAILED		= -2,
};
enum rseq_abi_flags {
	RSEQ_ABI_FLAG_UNREGISTER = (1 << 0),
};
enum rseq_abi_cs_flags_bit {
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT	= 0,
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT	= 1,
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT	= 2,
};
enum rseq_abi_cs_flags {
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_PREEMPT	=
		(1U << RSEQ_ABI_CS_FLAG_NO_RESTART_ON_PREEMPT_BIT),
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_SIGNAL	=
		(1U << RSEQ_ABI_CS_FLAG_NO_RESTART_ON_SIGNAL_BIT),
	RSEQ_ABI_CS_FLAG_NO_RESTART_ON_MIGRATE	=
		(1U << RSEQ_ABI_CS_FLAG_NO_RESTART_ON_MIGRATE_BIT),
};
struct rseq_abi_cs {
	__u32 version;
	__u32 flags;
	__u64 start_ip;
	__u64 post_commit_offset;
	__u64 abort_ip;
} __attribute__((aligned(4 * sizeof(__u64))));
struct rseq_abi {
	__u32 cpu_id_start;
	__u32 cpu_id;
	union {
		__u64 ptr64;
		struct {
#ifdef __LP64__
			__u64 ptr;
#elif defined(__BYTE_ORDER) ? (__BYTE_ORDER == __BIG_ENDIAN) : defined(__BIG_ENDIAN)
			__u32 padding;		 
			__u32 ptr;
#else
			__u32 ptr;
			__u32 padding;		 
#endif
		} arch;
	} rseq_cs;
	__u32 flags;
	__u32 node_id;
	__u32 mm_cid;
	char end[];
} __attribute__((aligned(4 * sizeof(__u64))));
#endif  
