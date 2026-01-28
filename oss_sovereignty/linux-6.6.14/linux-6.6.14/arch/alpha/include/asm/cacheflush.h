#ifndef _ALPHA_CACHEFLUSH_H
#define _ALPHA_CACHEFLUSH_H
#include <linux/mm.h>
#ifndef CONFIG_SMP
#define flush_icache_range(start, end)		imb()
#else
#define flush_icache_range(start, end)		smp_imb()
extern void smp_imb(void);
#endif
#ifndef CONFIG_SMP
#include <linux/sched.h>
extern void __load_new_mm_context(struct mm_struct *);
static inline void
flush_icache_user_page(struct vm_area_struct *vma, struct page *page,
			unsigned long addr, int len)
{
	if (vma->vm_flags & VM_EXEC) {
		struct mm_struct *mm = vma->vm_mm;
		if (current->active_mm == mm)
			__load_new_mm_context(mm);
		else
			mm->context[smp_processor_id()] = 0;
	}
}
#define flush_icache_user_page flush_icache_user_page
#else  
extern void flush_icache_user_page(struct vm_area_struct *vma,
		struct page *page, unsigned long addr, int len);
#define flush_icache_user_page flush_icache_user_page
#endif  
static inline void flush_icache_pages(struct vm_area_struct *vma,
		struct page *page, unsigned int nr)
{
	flush_icache_user_page(vma, page, 0, 0);
}
#define flush_icache_pages flush_icache_pages
#include <asm-generic/cacheflush.h>
#endif  
