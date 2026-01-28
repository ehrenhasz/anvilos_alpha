#ifndef MM_KFENCE_KFENCE_H
#define MM_KFENCE_KFENCE_H
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include "../slab.h"  
#define KFENCE_CANARY_PATTERN_U8(addr) ((u8)0xaa ^ (u8)((unsigned long)(addr) & 0x7))
#define KFENCE_CANARY_PATTERN_U64 ((u64)0xaaaaaaaaaaaaaaaa ^ (u64)(le64_to_cpu(0x0706050403020100)))
#define KFENCE_STACK_DEPTH 64
enum kfence_object_state {
	KFENCE_OBJECT_UNUSED,		 
	KFENCE_OBJECT_ALLOCATED,	 
	KFENCE_OBJECT_FREED,		 
};
struct kfence_track {
	pid_t pid;
	int cpu;
	u64 ts_nsec;
	int num_stack_entries;
	unsigned long stack_entries[KFENCE_STACK_DEPTH];
};
struct kfence_metadata {
	struct list_head list;		 
	struct rcu_head rcu_head;	 
	raw_spinlock_t lock;
	enum kfence_object_state state;
	unsigned long addr;
	size_t size;
	struct kmem_cache *cache;
	unsigned long unprotected_page;
	struct kfence_track alloc_track;
	struct kfence_track free_track;
	u32 alloc_stack_hash;
#ifdef CONFIG_MEMCG
	struct obj_cgroup *objcg;
#endif
};
#define KFENCE_METADATA_SIZE PAGE_ALIGN(sizeof(struct kfence_metadata) * \
					CONFIG_KFENCE_NUM_OBJECTS)
extern struct kfence_metadata *kfence_metadata;
static inline struct kfence_metadata *addr_to_metadata(unsigned long addr)
{
	long index;
	if (!is_kfence_address((void *)addr))
		return NULL;
	index = (addr - (unsigned long)__kfence_pool) / (PAGE_SIZE * 2) - 1;
	if (index < 0 || index >= CONFIG_KFENCE_NUM_OBJECTS)
		return NULL;
	return &kfence_metadata[index];
}
enum kfence_error_type {
	KFENCE_ERROR_OOB,		 
	KFENCE_ERROR_UAF,		 
	KFENCE_ERROR_CORRUPTION,	 
	KFENCE_ERROR_INVALID,		 
	KFENCE_ERROR_INVALID_FREE,	 
};
void kfence_report_error(unsigned long address, bool is_write, struct pt_regs *regs,
			 const struct kfence_metadata *meta, enum kfence_error_type type);
void kfence_print_object(struct seq_file *seq, const struct kfence_metadata *meta);
#endif  
