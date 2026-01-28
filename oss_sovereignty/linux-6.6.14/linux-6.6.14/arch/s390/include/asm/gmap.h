#ifndef _ASM_S390_GMAP_H
#define _ASM_S390_GMAP_H
#include <linux/radix-tree.h>
#include <linux/refcount.h>
#define GMAP_NOTIFY_SHADOW	0x2
#define GMAP_NOTIFY_MPROT	0x1
#define _SEGMENT_ENTRY_GMAP_IN		0x8000	 
#define _SEGMENT_ENTRY_GMAP_UC		0x4000	 
struct gmap {
	struct list_head list;
	struct list_head crst_list;
	struct mm_struct *mm;
	struct radix_tree_root guest_to_host;
	struct radix_tree_root host_to_guest;
	spinlock_t guest_table_lock;
	refcount_t ref_count;
	unsigned long *table;
	unsigned long asce;
	unsigned long asce_end;
	void *private;
	bool pfault_enabled;
	unsigned long guest_handle;
	struct radix_tree_root host_to_rmap;
	struct list_head children;
	struct list_head pt_list;
	spinlock_t shadow_lock;
	struct gmap *parent;
	unsigned long orig_asce;
	int edat_level;
	bool removed;
	bool initialized;
};
struct gmap_rmap {
	struct gmap_rmap *next;
	unsigned long raddr;
};
#define gmap_for_each_rmap(pos, head) \
	for (pos = (head); pos; pos = pos->next)
#define gmap_for_each_rmap_safe(pos, n, head) \
	for (pos = (head); n = pos ? pos->next : NULL, pos; pos = n)
struct gmap_notifier {
	struct list_head list;
	struct rcu_head rcu;
	void (*notifier_call)(struct gmap *gmap, unsigned long start,
			      unsigned long end);
};
static inline int gmap_is_shadow(struct gmap *gmap)
{
	return !!gmap->parent;
}
struct gmap *gmap_create(struct mm_struct *mm, unsigned long limit);
void gmap_remove(struct gmap *gmap);
struct gmap *gmap_get(struct gmap *gmap);
void gmap_put(struct gmap *gmap);
void gmap_enable(struct gmap *gmap);
void gmap_disable(struct gmap *gmap);
struct gmap *gmap_get_enabled(void);
int gmap_map_segment(struct gmap *gmap, unsigned long from,
		     unsigned long to, unsigned long len);
int gmap_unmap_segment(struct gmap *gmap, unsigned long to, unsigned long len);
unsigned long __gmap_translate(struct gmap *, unsigned long gaddr);
unsigned long gmap_translate(struct gmap *, unsigned long gaddr);
int __gmap_link(struct gmap *gmap, unsigned long gaddr, unsigned long vmaddr);
int gmap_fault(struct gmap *, unsigned long gaddr, unsigned int fault_flags);
void gmap_discard(struct gmap *, unsigned long from, unsigned long to);
void __gmap_zap(struct gmap *, unsigned long gaddr);
void gmap_unlink(struct mm_struct *, unsigned long *table, unsigned long vmaddr);
int gmap_read_table(struct gmap *gmap, unsigned long gaddr, unsigned long *val);
struct gmap *gmap_shadow(struct gmap *parent, unsigned long asce,
			 int edat_level);
int gmap_shadow_valid(struct gmap *sg, unsigned long asce, int edat_level);
int gmap_shadow_r2t(struct gmap *sg, unsigned long saddr, unsigned long r2t,
		    int fake);
int gmap_shadow_r3t(struct gmap *sg, unsigned long saddr, unsigned long r3t,
		    int fake);
int gmap_shadow_sgt(struct gmap *sg, unsigned long saddr, unsigned long sgt,
		    int fake);
int gmap_shadow_pgt(struct gmap *sg, unsigned long saddr, unsigned long pgt,
		    int fake);
int gmap_shadow_pgt_lookup(struct gmap *sg, unsigned long saddr,
			   unsigned long *pgt, int *dat_protection, int *fake);
int gmap_shadow_page(struct gmap *sg, unsigned long saddr, pte_t pte);
void gmap_register_pte_notifier(struct gmap_notifier *);
void gmap_unregister_pte_notifier(struct gmap_notifier *);
int gmap_mprotect_notify(struct gmap *, unsigned long start,
			 unsigned long len, int prot);
void gmap_sync_dirty_log_pmd(struct gmap *gmap, unsigned long dirty_bitmap[4],
			     unsigned long gaddr, unsigned long vmaddr);
int gmap_mark_unmergeable(void);
void s390_unlist_old_asce(struct gmap *gmap);
int s390_replace_asce(struct gmap *gmap);
void s390_uv_destroy_pfns(unsigned long count, unsigned long *pfns);
int __s390_uv_destroy_range(struct mm_struct *mm, unsigned long start,
			    unsigned long end, bool interruptible);
static inline void s390_uv_destroy_range(struct mm_struct *mm, unsigned long start,
					 unsigned long end)
{
	(void)__s390_uv_destroy_range(mm, start, end, false);
}
static inline int s390_uv_destroy_range_interruptible(struct mm_struct *mm, unsigned long start,
						      unsigned long end)
{
	return __s390_uv_destroy_range(mm, start, end, true);
}
#endif  
