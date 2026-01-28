#ifdef __ARMEB__
#define RSEQ_SIG    0xf3def5e7       
#else
#define RSEQ_SIG    0xe7f5def3       
#endif
#define rseq_smp_mb()	__asm__ __volatile__ ("dmb" ::: "memory", "cc")
#define rseq_smp_rmb()	__asm__ __volatile__ ("dmb" ::: "memory", "cc")
#define rseq_smp_wmb()	__asm__ __volatile__ ("dmb" ::: "memory", "cc")
#define rseq_smp_load_acquire(p)					\
__extension__ ({							\
	rseq_unqual_scalar_typeof(*(p)) ____p1 = RSEQ_READ_ONCE(*(p));	\
	rseq_smp_mb();							\
	____p1;								\
})
#define rseq_smp_acquire__after_ctrl_dep()	rseq_smp_rmb()
#define rseq_smp_store_release(p, v)					\
do {									\
	rseq_smp_mb();							\
	RSEQ_WRITE_ONCE(*(p), v);					\
} while (0)
#define __RSEQ_ASM_DEFINE_TABLE(label, version, flags, start_ip,	\
				post_commit_offset, abort_ip)		\
		".pushsection __rseq_cs, \"aw\"\n\t"			\
		".balign 32\n\t"					\
		__rseq_str(label) ":\n\t"					\
		".word " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".word " __rseq_str(start_ip) ", 0x0, " __rseq_str(post_commit_offset) ", 0x0, " __rseq_str(abort_ip) ", 0x0\n\t" \
		".popsection\n\t"					\
		".pushsection __rseq_cs_ptr_array, \"aw\"\n\t"		\
		".word " __rseq_str(label) "b, 0x0\n\t"			\
		".popsection\n\t"
#define RSEQ_ASM_DEFINE_TABLE(label, start_ip, post_commit_ip, abort_ip) \
	__RSEQ_ASM_DEFINE_TABLE(label, 0x0, 0x0, start_ip,		\
				(post_commit_ip - start_ip), abort_ip)
#define RSEQ_ASM_DEFINE_EXIT_POINT(start_ip, exit_ip)			\
		".pushsection __rseq_exit_point_array, \"aw\"\n\t"	\
		".word " __rseq_str(start_ip) ", 0x0, " __rseq_str(exit_ip) ", 0x0\n\t" \
		".popsection\n\t"
#define RSEQ_ASM_STORE_RSEQ_CS(label, cs_label, rseq_cs)		\
		RSEQ_INJECT_ASM(1)					\
		"adr r0, " __rseq_str(cs_label) "\n\t"			\
		"str r0, %[" __rseq_str(rseq_cs) "]\n\t"		\
		__rseq_str(label) ":\n\t"
#define RSEQ_ASM_CMP_CPU_ID(cpu_id, current_cpu_id, label)		\
		RSEQ_INJECT_ASM(2)					\
		"ldr r0, %[" __rseq_str(current_cpu_id) "]\n\t"	\
		"cmp %[" __rseq_str(cpu_id) "], r0\n\t"		\
		"bne " __rseq_str(label) "\n\t"
#define __RSEQ_ASM_DEFINE_ABORT(table_label, label, teardown,		\
				abort_label, version, flags,		\
				start_ip, post_commit_offset, abort_ip)	\
		".balign 32\n\t"					\
		__rseq_str(table_label) ":\n\t"				\
		".word " __rseq_str(version) ", " __rseq_str(flags) "\n\t" \
		".word " __rseq_str(start_ip) ", 0x0, " __rseq_str(post_commit_offset) ", 0x0, " __rseq_str(abort_ip) ", 0x0\n\t" \
		".word " __rseq_str(RSEQ_SIG) "\n\t"			\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"b %l[" __rseq_str(abort_label) "]\n\t"
#define RSEQ_ASM_DEFINE_ABORT(table_label, label, teardown, abort_label, \
			      start_ip, post_commit_ip, abort_ip)	\
	__RSEQ_ASM_DEFINE_ABORT(table_label, label, teardown,		\
				abort_label, 0x0, 0x0, start_ip,	\
				(post_commit_ip - start_ip), abort_ip)
#define RSEQ_ASM_DEFINE_CMPFAIL(label, teardown, cmpfail_label)		\
		__rseq_str(label) ":\n\t"				\
		teardown						\
		"b %l[" __rseq_str(cmpfail_label) "]\n\t"
#define RSEQ_TEMPLATE_CPU_ID
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-arm-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED
#define RSEQ_TEMPLATE_MO_RELEASE
#include "rseq-arm-bits.h"
#undef RSEQ_TEMPLATE_MO_RELEASE
#undef RSEQ_TEMPLATE_CPU_ID
#define RSEQ_TEMPLATE_MM_CID
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-arm-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED
#define RSEQ_TEMPLATE_MO_RELEASE
#include "rseq-arm-bits.h"
#undef RSEQ_TEMPLATE_MO_RELEASE
#undef RSEQ_TEMPLATE_MM_CID
#define RSEQ_TEMPLATE_CPU_ID_NONE
#define RSEQ_TEMPLATE_MO_RELAXED
#include "rseq-arm-bits.h"
#undef RSEQ_TEMPLATE_MO_RELAXED
#undef RSEQ_TEMPLATE_CPU_ID_NONE
