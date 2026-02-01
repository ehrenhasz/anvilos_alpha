 
 

#ifndef RSEQ_H
#error "Never use <rseq-x86.h> directly; include <rseq.h> instead."
#endif

#include <stdint.h>

 
#define RSEQ_SIG	0x53053053

 

 
#define RSEQ_CPU_ID_OFFSET	4
#define RSEQ_CS_OFFSET		8
#define RSEQ_MM_CID_OFFSET	24

#ifdef __x86_64__

#define RSEQ_ASM_TP_SEGMENT	%%fs

#define rseq_smp_mb()	\
	__asm__ __volatile__ ("lock; addl $0,-128(%%rsp)" ::: "memory", "cc")
#define rseq_smp_rmb()	rseq_barrier()
#define rseq_smp_wmb()	rseq_barrier()

#define rseq_smp_load_acquire(p)					\
__extension__ ({							\
	rseq_unqual_scalar_typeof(*(p)) ____p1 = RSEQ_READ_ONCE(*(p));	\
	rseq_barrier();							\
	____p1;								\
})

#define rseq_smp_acquire__after_ctrl_dep()	rseq_smp_rmb()

#define rseq_smp_store_release(p, v)					\
do {									\
	rseq_barrier();							\
	RSEQ_WRITE_ONCE(*(p), v);					\
} while (0)

#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags,			\
				start_ip, post_commit_offset, abort_ip)	\
		".pushsection __rseq_cs, \"aw\"\n\t"			\
		".balign 32\n\t"					\
		__rseq_str(label) ":\n\t"				\
		".long " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".quad " __rseq_str(start_ip) ", " __rseq_str(post_commit_offset) ", " __rseq_str(abort_ip) "\n\t" \
		".popsection\n\t"					\
		".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"		\
		".quad " __rseq_str(label) "b\n\t"			\
		".popsection\n\t"


#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip) \
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,		\
				(post_commit_ip - start_ip), abort_ip)

 
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)			\
		".pushsection __rseq_exit_point_array, \"aw\"\n\t"	\
		".quad " __rseq_str(start_ip) ", " __rseq_str(exit_ip) "\n\t" \
		".popsection\n\t"

#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)		\
		RSEQ_INJECT_ASM(1)					\
		"leaq " __rseq_str(cs_label) "(%%rip), %%rax\n\t"	\
		"movq %%rax, " __rseq_str(rseq_cs) "\n\t"		\
		__rseq_str(label) ":\n\t"

#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)		\
		RSEQ_INJECT_ASM(2)					\
		"cmpl %[" __rseq_str(cpu_id) "], " __rseq_str(current_cpu_id) "\n\t" \
		"jnz " __rseq_str(label) "\n\t"

#define RSEQ_ASM_DEFINE_ABORT(label, teardown, abort_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		  \
		".byte 0x0f, 0xb9, 0x3d\n\t"				\
		".long " __rseq_str(RSEQ_SIG) "\n\t"			\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(abort_label) "]\n\t"		\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_CMPFAIL(label, teardown, cmpfail_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(cmpfail_label) "]\n\t"		\
		".popsection\n\t"

#elif defined(__i386__)

#define RSEQ_ASM_TP_SEGMENT	%%gs

#define rseq_smp_mb()	\
	__asm__ __volatile__ ("lock; addl $0,-128(%%esp)" ::: "memory", "cc")
#define rseq_smp_rmb()	\
	__asm__ __volatile__ ("lock; addl $0,-128(%%esp)" ::: "memory", "cc")
#define rseq_smp_wmb()	\
	__asm__ __volatile__ ("lock; addl $0,-128(%%esp)" ::: "memory", "cc")

#define rseq_smp_load_acquire(p)					\
__extension__ ({							\
	__typeof(*p) ____p1 = RSEQ_READ_ONCE(*p);			\
	rseq_smp_mb();							\
	____p1;								\
})

#define rseq_smp_acquire__after_ctrl_dep()	rseq_smp_rmb()

#define rseq_smp_store_release(p, v)					\
do {									\
	rseq_smp_mb();							\
	RSEQ_WRITE_ONCE(*p, v);						\
} while (0)

 
#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags,			\
				start_ip, post_commit_offset, abort_ip)	\
		".pushsection __rseq_cs, \"aw\"\n\t"			\
		".balign 32\n\t"					\
		__rseq_str(label) ":\n\t"				\
		".long " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".long " __rseq_str(start_ip) ", 0x0, " __rseq_str(post_commit_offset) ", 0x0, " __rseq_str(abort_ip) ", 0x0\n\t" \
		".popsection\n\t"					\
		".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"		\
		".long " __rseq_str(label) "b, 0x0\n\t"			\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip) \
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,		\
				(post_commit_ip - start_ip), abort_ip)

 
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)			\
		".pushsection __rseq_exit_point_array, \"aw\"\n\t"	\
		".long " __rseq_str(start_ip) ", 0x0, " __rseq_str(exit_ip) ", 0x0\n\t" \
		".popsection\n\t"

#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)		\
		RSEQ_INJECT_ASM(1)					\
		"movl $" __rseq_str(cs_label) ", " __rseq_str(rseq_cs) "\n\t"	\
		__rseq_str(label) ":\n\t"

#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)		\
		RSEQ_INJECT_ASM(2)					\
		"cmpl %[" __rseq_str(cpu_id) "], " __rseq_str(current_cpu_id) "\n\t" \
		"jnz " __rseq_str(label) "\n\t"

#define RSEQ_ASM_DEFINE_ABORT(label, teardown, abort_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		 	\
		".byte 0x0f, 0xb9, 0x3d\n\t"				\
		".long " __rseq_str(RSEQ_SIG) "\n\t"			\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(abort_label) "]\n\t"		\
		".popsection\n\t"

#define RSEQ_ASM_DEFINE_CMPFAIL(label, teardown, cmpfail_label)		\
		".pushsection __rseq_failure, \"ax\"\n\t"		\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"jmp %l[" __rseq_str(cmpfail_label) "]\n\t"		\
		".popsection\n\t"

#endif

 

#define RSEQ_TEMPLATE_CPU_ID
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-x86-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED

#define RSEQ_TEMPLATE_MO_RELEASE
#include "rseq-x86-bits.h"
#undef RSEQ_TEMPLATE_MO_RELEASE
#undef RSEQ_TEMPLATE_CPU_ID

 

#define RSEQ_TEMPLATE_MM_CID
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-x86-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED

#define RSEQ_TEMPLATE_MO_RELEASE
#include "rseq-x86-bits.h"
#undef RSEQ_TEMPLATE_MO_RELEASE
#undef RSEQ_TEMPLATE_MM_CID

 

#define RSEQ_TEMPLATE_CPU_ID_NONE
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-x86-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED
#undef RSEQ_TEMPLATE_CPU_ID_NONE
