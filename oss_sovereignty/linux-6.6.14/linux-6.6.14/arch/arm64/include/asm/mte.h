#ifndef __ASM_MTE_H
#define __ASM_MTE_H
#include <asm/compiler.h>
#include <asm/mte-def.h>
#ifndef __ASSEMBLY__
#include <linux/bitfield.h>
#include <linux/kasan-enabled.h>
#include <linux/page-flags.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <asm/pgtable-types.h>
void mte_clear_page_tags(void *addr);
unsigned long mte_copy_tags_from_user(void *to, const void __user *from,
				      unsigned long n);
unsigned long mte_copy_tags_to_user(void __user *to, void *from,
				    unsigned long n);
int mte_save_tags(struct page *page);
void mte_save_page_tags(const void *page_addr, void *tag_storage);
void mte_restore_tags(swp_entry_t entry, struct page *page);
void mte_restore_page_tags(void *page_addr, const void *tag_storage);
void mte_invalidate_tags(int type, pgoff_t offset);
void mte_invalidate_tags_area(int type);
void *mte_allocate_tag_storage(void);
void mte_free_tag_storage(char *storage);
#ifdef CONFIG_ARM64_MTE
#define PG_mte_tagged	PG_arch_2
#define PG_mte_lock	PG_arch_3
static inline void set_page_mte_tagged(struct page *page)
{
	smp_wmb();
	set_bit(PG_mte_tagged, &page->flags);
}
static inline bool page_mte_tagged(struct page *page)
{
	bool ret = test_bit(PG_mte_tagged, &page->flags);
	if (ret)
		smp_rmb();
	return ret;
}
static inline bool try_page_mte_tagging(struct page *page)
{
	if (!test_and_set_bit(PG_mte_lock, &page->flags))
		return true;
	smp_cond_load_acquire(&page->flags, VAL & (1UL << PG_mte_tagged));
	return false;
}
void mte_zero_clear_page_tags(void *addr);
void mte_sync_tags(pte_t pte);
void mte_copy_page_tags(void *kto, const void *kfrom);
void mte_thread_init_user(void);
void mte_thread_switch(struct task_struct *next);
void mte_cpu_setup(void);
void mte_suspend_enter(void);
void mte_suspend_exit(void);
long set_mte_ctrl(struct task_struct *task, unsigned long arg);
long get_mte_ctrl(struct task_struct *task);
int mte_ptrace_copy_tags(struct task_struct *child, long request,
			 unsigned long addr, unsigned long data);
size_t mte_probe_user_range(const char __user *uaddr, size_t size);
#else  
#define PG_mte_tagged	0
static inline void set_page_mte_tagged(struct page *page)
{
}
static inline bool page_mte_tagged(struct page *page)
{
	return false;
}
static inline bool try_page_mte_tagging(struct page *page)
{
	return false;
}
static inline void mte_zero_clear_page_tags(void *addr)
{
}
static inline void mte_sync_tags(pte_t pte)
{
}
static inline void mte_copy_page_tags(void *kto, const void *kfrom)
{
}
static inline void mte_thread_init_user(void)
{
}
static inline void mte_thread_switch(struct task_struct *next)
{
}
static inline void mte_suspend_enter(void)
{
}
static inline void mte_suspend_exit(void)
{
}
static inline long set_mte_ctrl(struct task_struct *task, unsigned long arg)
{
	return 0;
}
static inline long get_mte_ctrl(struct task_struct *task)
{
	return 0;
}
static inline int mte_ptrace_copy_tags(struct task_struct *child,
				       long request, unsigned long addr,
				       unsigned long data)
{
	return -EIO;
}
#endif  
static inline void mte_disable_tco_entry(struct task_struct *task)
{
	if (!system_supports_mte())
		return;
	if (kasan_hw_tags_enabled() ||
	    (task->thread.sctlr_user & (1UL << SCTLR_EL1_TCF0_SHIFT)))
		asm volatile(SET_PSTATE_TCO(0));
}
#ifdef CONFIG_KASAN_HW_TAGS
void mte_check_tfsr_el1(void);
static inline void mte_check_tfsr_entry(void)
{
	if (!system_supports_mte())
		return;
	mte_check_tfsr_el1();
}
static inline void mte_check_tfsr_exit(void)
{
	if (!system_supports_mte())
		return;
	dsb(nsh);
	isb();
	mte_check_tfsr_el1();
}
#else
static inline void mte_check_tfsr_el1(void)
{
}
static inline void mte_check_tfsr_entry(void)
{
}
static inline void mte_check_tfsr_exit(void)
{
}
#endif  
#endif  
#endif  
