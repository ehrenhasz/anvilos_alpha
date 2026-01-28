#ifndef __MMU_H
#define __MMU_H
#include <linux/cpumask.h>
#include <linux/errno.h>
#include <asm/asm-extable.h>
typedef struct {
	spinlock_t lock;
	cpumask_t cpu_attach_mask;
	atomic_t flush_count;
	unsigned int flush_mm;
	struct list_head pgtable_list;
	struct list_head gmap_list;
	unsigned long gmap_asce;
	unsigned long asce;
	unsigned long asce_limit;
	unsigned long vdso_base;
	atomic_t protected_count;
	unsigned int alloc_pgste:1;
	unsigned int has_pgste:1;
	unsigned int uses_skeys:1;
	unsigned int uses_cmm:1;
	unsigned int allow_gmap_hpage_1m:1;
} mm_context_t;
#define INIT_MM_CONTEXT(name)						   \
	.context.lock =	__SPIN_LOCK_UNLOCKED(name.context.lock),	   \
	.context.pgtable_list = LIST_HEAD_INIT(name.context.pgtable_list), \
	.context.gmap_list = LIST_HEAD_INIT(name.context.gmap_list),
#endif
