
 

#include <linux/init.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/kfence.h>
#include <linux/kmemleak.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

#include "kasan.h"

bool __kasan_check_read(const volatile void *p, unsigned int size)
{
	return kasan_check_range((void *)p, size, false, _RET_IP_);
}
EXPORT_SYMBOL(__kasan_check_read);

bool __kasan_check_write(const volatile void *p, unsigned int size)
{
	return kasan_check_range((void *)p, size, true, _RET_IP_);
}
EXPORT_SYMBOL(__kasan_check_write);

#if !defined(CONFIG_CC_HAS_KASAN_MEMINTRINSIC_PREFIX) && !defined(CONFIG_GENERIC_ENTRY)
 
#undef memset
void *memset(void *addr, int c, size_t len)
{
	if (!kasan_check_range(addr, len, true, _RET_IP_))
		return NULL;

	return __memset(addr, c, len);
}

#ifdef __HAVE_ARCH_MEMMOVE
#undef memmove
void *memmove(void *dest, const void *src, size_t len)
{
	if (!kasan_check_range(src, len, false, _RET_IP_) ||
	    !kasan_check_range(dest, len, true, _RET_IP_))
		return NULL;

	return __memmove(dest, src, len);
}
#endif

#undef memcpy
void *memcpy(void *dest, const void *src, size_t len)
{
	if (!kasan_check_range(src, len, false, _RET_IP_) ||
	    !kasan_check_range(dest, len, true, _RET_IP_))
		return NULL;

	return __memcpy(dest, src, len);
}
#endif

void *__asan_memset(void *addr, int c, ssize_t len)
{
	if (!kasan_check_range(addr, len, true, _RET_IP_))
		return NULL;

	return __memset(addr, c, len);
}
EXPORT_SYMBOL(__asan_memset);

#ifdef __HAVE_ARCH_MEMMOVE
void *__asan_memmove(void *dest, const void *src, ssize_t len)
{
	if (!kasan_check_range(src, len, false, _RET_IP_) ||
	    !kasan_check_range(dest, len, true, _RET_IP_))
		return NULL;

	return __memmove(dest, src, len);
}
EXPORT_SYMBOL(__asan_memmove);
#endif

void *__asan_memcpy(void *dest, const void *src, ssize_t len)
{
	if (!kasan_check_range(src, len, false, _RET_IP_) ||
	    !kasan_check_range(dest, len, true, _RET_IP_))
		return NULL;

	return __memcpy(dest, src, len);
}
EXPORT_SYMBOL(__asan_memcpy);

#ifdef CONFIG_KASAN_SW_TAGS
void *__hwasan_memset(void *addr, int c, ssize_t len) __alias(__asan_memset);
EXPORT_SYMBOL(__hwasan_memset);
#ifdef __HAVE_ARCH_MEMMOVE
void *__hwasan_memmove(void *dest, const void *src, ssize_t len) __alias(__asan_memmove);
EXPORT_SYMBOL(__hwasan_memmove);
#endif
void *__hwasan_memcpy(void *dest, const void *src, ssize_t len) __alias(__asan_memcpy);
EXPORT_SYMBOL(__hwasan_memcpy);
#endif

void kasan_poison(const void *addr, size_t size, u8 value, bool init)
{
	void *shadow_start, *shadow_end;

	if (!kasan_arch_is_ready())
		return;

	 
	addr = kasan_reset_tag(addr);

	 
	if (is_kfence_address(addr))
		return;

	if (WARN_ON((unsigned long)addr & KASAN_GRANULE_MASK))
		return;
	if (WARN_ON(size & KASAN_GRANULE_MASK))
		return;

	shadow_start = kasan_mem_to_shadow(addr);
	shadow_end = kasan_mem_to_shadow(addr + size);

	__memset(shadow_start, value, shadow_end - shadow_start);
}
EXPORT_SYMBOL(kasan_poison);

#ifdef CONFIG_KASAN_GENERIC
void kasan_poison_last_granule(const void *addr, size_t size)
{
	if (!kasan_arch_is_ready())
		return;

	if (size & KASAN_GRANULE_MASK) {
		u8 *shadow = (u8 *)kasan_mem_to_shadow(addr + size);
		*shadow = size & KASAN_GRANULE_MASK;
	}
}
#endif

void kasan_unpoison(const void *addr, size_t size, bool init)
{
	u8 tag = get_tag(addr);

	 
	addr = kasan_reset_tag(addr);

	 
	if (is_kfence_address(addr))
		return;

	if (WARN_ON((unsigned long)addr & KASAN_GRANULE_MASK))
		return;

	 
	kasan_poison(addr, round_up(size, KASAN_GRANULE_SIZE), tag, false);

	 
	if (IS_ENABLED(CONFIG_KASAN_GENERIC))
		kasan_poison_last_granule(addr, size);
}

#ifdef CONFIG_MEMORY_HOTPLUG
static bool shadow_mapped(unsigned long addr)
{
	pgd_t *pgd = pgd_offset_k(addr);
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (pgd_none(*pgd))
		return false;
	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d))
		return false;
	pud = pud_offset(p4d, addr);
	if (pud_none(*pud))
		return false;

	 
	if (pud_bad(*pud))
		return true;
	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return false;

	if (pmd_bad(*pmd))
		return true;
	pte = pte_offset_kernel(pmd, addr);
	return !pte_none(ptep_get(pte));
}

static int __meminit kasan_mem_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct memory_notify *mem_data = data;
	unsigned long nr_shadow_pages, start_kaddr, shadow_start;
	unsigned long shadow_end, shadow_size;

	nr_shadow_pages = mem_data->nr_pages >> KASAN_SHADOW_SCALE_SHIFT;
	start_kaddr = (unsigned long)pfn_to_kaddr(mem_data->start_pfn);
	shadow_start = (unsigned long)kasan_mem_to_shadow((void *)start_kaddr);
	shadow_size = nr_shadow_pages << PAGE_SHIFT;
	shadow_end = shadow_start + shadow_size;

	if (WARN_ON(mem_data->nr_pages % KASAN_GRANULE_SIZE) ||
		WARN_ON(start_kaddr % KASAN_MEMORY_PER_SHADOW_PAGE))
		return NOTIFY_BAD;

	switch (action) {
	case MEM_GOING_ONLINE: {
		void *ret;

		 
		if (shadow_mapped(shadow_start))
			return NOTIFY_OK;

		ret = __vmalloc_node_range(shadow_size, PAGE_SIZE, shadow_start,
					shadow_end, GFP_KERNEL,
					PAGE_KERNEL, VM_NO_GUARD,
					pfn_to_nid(mem_data->start_pfn),
					__builtin_return_address(0));
		if (!ret)
			return NOTIFY_BAD;

		kmemleak_ignore(ret);
		return NOTIFY_OK;
	}
	case MEM_CANCEL_ONLINE:
	case MEM_OFFLINE: {
		struct vm_struct *vm;

		 
		vm = find_vm_area((void *)shadow_start);
		if (vm)
			vfree((void *)shadow_start);
	}
	}

	return NOTIFY_OK;
}

static int __init kasan_memhotplug_init(void)
{
	hotplug_memory_notifier(kasan_mem_notifier, DEFAULT_CALLBACK_PRI);

	return 0;
}

core_initcall(kasan_memhotplug_init);
#endif

#ifdef CONFIG_KASAN_VMALLOC

void __init __weak kasan_populate_early_vm_area_shadow(void *start,
						       unsigned long size)
{
}

static int kasan_populate_vmalloc_pte(pte_t *ptep, unsigned long addr,
				      void *unused)
{
	unsigned long page;
	pte_t pte;

	if (likely(!pte_none(ptep_get(ptep))))
		return 0;

	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	memset((void *)page, KASAN_VMALLOC_INVALID, PAGE_SIZE);
	pte = pfn_pte(PFN_DOWN(__pa(page)), PAGE_KERNEL);

	spin_lock(&init_mm.page_table_lock);
	if (likely(pte_none(ptep_get(ptep)))) {
		set_pte_at(&init_mm, addr, ptep, pte);
		page = 0;
	}
	spin_unlock(&init_mm.page_table_lock);
	if (page)
		free_page(page);
	return 0;
}

int kasan_populate_vmalloc(unsigned long addr, unsigned long size)
{
	unsigned long shadow_start, shadow_end;
	int ret;

	if (!kasan_arch_is_ready())
		return 0;

	if (!is_vmalloc_or_module_addr((void *)addr))
		return 0;

	shadow_start = (unsigned long)kasan_mem_to_shadow((void *)addr);
	shadow_end = (unsigned long)kasan_mem_to_shadow((void *)addr + size);

	 
	if (IS_ENABLED(CONFIG_UML)) {
		__memset((void *)shadow_start, KASAN_VMALLOC_INVALID, shadow_end - shadow_start);
		return 0;
	}

	shadow_start = PAGE_ALIGN_DOWN(shadow_start);
	shadow_end = PAGE_ALIGN(shadow_end);

	ret = apply_to_page_range(&init_mm, shadow_start,
				  shadow_end - shadow_start,
				  kasan_populate_vmalloc_pte, NULL);
	if (ret)
		return ret;

	flush_cache_vmap(shadow_start, shadow_end);

	 

	return 0;
}

static int kasan_depopulate_vmalloc_pte(pte_t *ptep, unsigned long addr,
					void *unused)
{
	unsigned long page;

	page = (unsigned long)__va(pte_pfn(ptep_get(ptep)) << PAGE_SHIFT);

	spin_lock(&init_mm.page_table_lock);

	if (likely(!pte_none(ptep_get(ptep)))) {
		pte_clear(&init_mm, addr, ptep);
		free_page(page);
	}
	spin_unlock(&init_mm.page_table_lock);

	return 0;
}

 
void kasan_release_vmalloc(unsigned long start, unsigned long end,
			   unsigned long free_region_start,
			   unsigned long free_region_end)
{
	void *shadow_start, *shadow_end;
	unsigned long region_start, region_end;
	unsigned long size;

	if (!kasan_arch_is_ready())
		return;

	region_start = ALIGN(start, KASAN_MEMORY_PER_SHADOW_PAGE);
	region_end = ALIGN_DOWN(end, KASAN_MEMORY_PER_SHADOW_PAGE);

	free_region_start = ALIGN(free_region_start, KASAN_MEMORY_PER_SHADOW_PAGE);

	if (start != region_start &&
	    free_region_start < region_start)
		region_start -= KASAN_MEMORY_PER_SHADOW_PAGE;

	free_region_end = ALIGN_DOWN(free_region_end, KASAN_MEMORY_PER_SHADOW_PAGE);

	if (end != region_end &&
	    free_region_end > region_end)
		region_end += KASAN_MEMORY_PER_SHADOW_PAGE;

	shadow_start = kasan_mem_to_shadow((void *)region_start);
	shadow_end = kasan_mem_to_shadow((void *)region_end);

	if (shadow_end > shadow_start) {
		size = shadow_end - shadow_start;
		if (IS_ENABLED(CONFIG_UML)) {
			__memset(shadow_start, KASAN_SHADOW_INIT, shadow_end - shadow_start);
			return;
		}
		apply_to_existing_page_range(&init_mm,
					     (unsigned long)shadow_start,
					     size, kasan_depopulate_vmalloc_pte,
					     NULL);
		flush_tlb_kernel_range((unsigned long)shadow_start,
				       (unsigned long)shadow_end);
	}
}

void *__kasan_unpoison_vmalloc(const void *start, unsigned long size,
			       kasan_vmalloc_flags_t flags)
{
	 

	if (!kasan_arch_is_ready())
		return (void *)start;

	if (!is_vmalloc_or_module_addr(start))
		return (void *)start;

	 
	if (IS_ENABLED(CONFIG_KASAN_SW_TAGS) &&
	    !(flags & KASAN_VMALLOC_PROT_NORMAL))
		return (void *)start;

	start = set_tag(start, kasan_random_tag());
	kasan_unpoison(start, size, false);
	return (void *)start;
}

 
void __kasan_poison_vmalloc(const void *start, unsigned long size)
{
	if (!kasan_arch_is_ready())
		return;

	if (!is_vmalloc_or_module_addr(start))
		return;

	size = round_up(size, KASAN_GRANULE_SIZE);
	kasan_poison(start, size, KASAN_VMALLOC_INVALID, false);
}

#else  

int kasan_alloc_module_shadow(void *addr, size_t size, gfp_t gfp_mask)
{
	void *ret;
	size_t scaled_size;
	size_t shadow_size;
	unsigned long shadow_start;

	shadow_start = (unsigned long)kasan_mem_to_shadow(addr);
	scaled_size = (size + KASAN_GRANULE_SIZE - 1) >>
				KASAN_SHADOW_SCALE_SHIFT;
	shadow_size = round_up(scaled_size, PAGE_SIZE);

	if (WARN_ON(!PAGE_ALIGNED(shadow_start)))
		return -EINVAL;

	if (IS_ENABLED(CONFIG_UML)) {
		__memset((void *)shadow_start, KASAN_SHADOW_INIT, shadow_size);
		return 0;
	}

	ret = __vmalloc_node_range(shadow_size, 1, shadow_start,
			shadow_start + shadow_size,
			GFP_KERNEL,
			PAGE_KERNEL, VM_NO_GUARD, NUMA_NO_NODE,
			__builtin_return_address(0));

	if (ret) {
		struct vm_struct *vm = find_vm_area(addr);
		__memset(ret, KASAN_SHADOW_INIT, shadow_size);
		vm->flags |= VM_KASAN;
		kmemleak_ignore(ret);

		if (vm->flags & VM_DEFER_KMEMLEAK)
			kmemleak_vmalloc(vm, size, gfp_mask);

		return 0;
	}

	return -ENOMEM;
}

void kasan_free_module_shadow(const struct vm_struct *vm)
{
	if (IS_ENABLED(CONFIG_UML))
		return;

	if (vm->flags & VM_KASAN)
		vfree(kasan_mem_to_shadow(vm->addr));
}

#endif
