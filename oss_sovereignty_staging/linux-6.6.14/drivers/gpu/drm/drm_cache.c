 
 
#include <linux/cc_platform.h>
#include <linux/export.h>
#include <linux/highmem.h>
#include <linux/ioport.h>
#include <linux/iosys-map.h>
#include <xen/xen.h>

#include <drm/drm_cache.h>

 
#define MEMCPY_BOUNCE_SIZE 128

#if defined(CONFIG_X86)
#include <asm/smp.h>

 
static void
drm_clflush_page(struct page *page)
{
	uint8_t *page_virtual;
	unsigned int i;
	const int size = boot_cpu_data.x86_clflush_size;

	if (unlikely(page == NULL))
		return;

	page_virtual = kmap_atomic(page);
	for (i = 0; i < PAGE_SIZE; i += size)
		clflushopt(page_virtual + i);
	kunmap_atomic(page_virtual);
}

static void drm_cache_flush_clflush(struct page *pages[],
				    unsigned long num_pages)
{
	unsigned long i;

	mb();  
	for (i = 0; i < num_pages; i++)
		drm_clflush_page(*pages++);
	mb();  
}
#endif

 
void
drm_clflush_pages(struct page *pages[], unsigned long num_pages)
{

#if defined(CONFIG_X86)
	if (static_cpu_has(X86_FEATURE_CLFLUSH)) {
		drm_cache_flush_clflush(pages, num_pages);
		return;
	}

	if (wbinvd_on_all_cpus())
		pr_err("Timed out waiting for cache flush\n");

#elif defined(__powerpc__)
	unsigned long i;

	for (i = 0; i < num_pages; i++) {
		struct page *page = pages[i];
		void *page_virtual;

		if (unlikely(page == NULL))
			continue;

		page_virtual = kmap_atomic(page);
		flush_dcache_range((unsigned long)page_virtual,
				   (unsigned long)page_virtual + PAGE_SIZE);
		kunmap_atomic(page_virtual);
	}
#else
	WARN_ONCE(1, "Architecture has no drm_cache.c support\n");
#endif
}
EXPORT_SYMBOL(drm_clflush_pages);

 
void
drm_clflush_sg(struct sg_table *st)
{
#if defined(CONFIG_X86)
	if (static_cpu_has(X86_FEATURE_CLFLUSH)) {
		struct sg_page_iter sg_iter;

		mb();  
		for_each_sgtable_page(st, &sg_iter, 0)
			drm_clflush_page(sg_page_iter_page(&sg_iter));
		mb();  

		return;
	}

	if (wbinvd_on_all_cpus())
		pr_err("Timed out waiting for cache flush\n");
#else
	WARN_ONCE(1, "Architecture has no drm_cache.c support\n");
#endif
}
EXPORT_SYMBOL(drm_clflush_sg);

 
void
drm_clflush_virt_range(void *addr, unsigned long length)
{
#if defined(CONFIG_X86)
	if (static_cpu_has(X86_FEATURE_CLFLUSH)) {
		const int size = boot_cpu_data.x86_clflush_size;
		void *end = addr + length;

		addr = (void *)(((unsigned long)addr) & -size);
		mb();  
		for (; addr < end; addr += size)
			clflushopt(addr);
		clflushopt(end - 1);  
		mb();  
		return;
	}

	if (wbinvd_on_all_cpus())
		pr_err("Timed out waiting for cache flush\n");
#else
	WARN_ONCE(1, "Architecture has no drm_cache.c support\n");
#endif
}
EXPORT_SYMBOL(drm_clflush_virt_range);

bool drm_need_swiotlb(int dma_bits)
{
	struct resource *tmp;
	resource_size_t max_iomem = 0;

	 
	if (xen_pv_domain())
		return true;

	 
	if (cc_platform_has(CC_ATTR_MEM_ENCRYPT))
		return true;

	for (tmp = iomem_resource.child; tmp; tmp = tmp->sibling)
		max_iomem = max(max_iomem,  tmp->end);

	return max_iomem > ((u64)1 << dma_bits);
}
EXPORT_SYMBOL(drm_need_swiotlb);

static void memcpy_fallback(struct iosys_map *dst,
			    const struct iosys_map *src,
			    unsigned long len)
{
	if (!dst->is_iomem && !src->is_iomem) {
		memcpy(dst->vaddr, src->vaddr, len);
	} else if (!src->is_iomem) {
		iosys_map_memcpy_to(dst, 0, src->vaddr, len);
	} else if (!dst->is_iomem) {
		memcpy_fromio(dst->vaddr, src->vaddr_iomem, len);
	} else {
		 
		char bounce[MEMCPY_BOUNCE_SIZE];
		void __iomem *_src = src->vaddr_iomem;
		void __iomem *_dst = dst->vaddr_iomem;

		while (len >= MEMCPY_BOUNCE_SIZE) {
			memcpy_fromio(bounce, _src, MEMCPY_BOUNCE_SIZE);
			memcpy_toio(_dst, bounce, MEMCPY_BOUNCE_SIZE);
			_src += MEMCPY_BOUNCE_SIZE;
			_dst += MEMCPY_BOUNCE_SIZE;
			len -= MEMCPY_BOUNCE_SIZE;
		}
		if (len) {
			memcpy_fromio(bounce, _src, MEMCPY_BOUNCE_SIZE);
			memcpy_toio(_dst, bounce, MEMCPY_BOUNCE_SIZE);
		}
	}
}

#ifdef CONFIG_X86

static DEFINE_STATIC_KEY_FALSE(has_movntdqa);

static void __memcpy_ntdqa(void *dst, const void *src, unsigned long len)
{
	kernel_fpu_begin();

	while (len >= 4) {
		asm("movntdqa	(%0), %%xmm0\n"
		    "movntdqa 16(%0), %%xmm1\n"
		    "movntdqa 32(%0), %%xmm2\n"
		    "movntdqa 48(%0), %%xmm3\n"
		    "movaps %%xmm0,   (%1)\n"
		    "movaps %%xmm1, 16(%1)\n"
		    "movaps %%xmm2, 32(%1)\n"
		    "movaps %%xmm3, 48(%1)\n"
		    :: "r" (src), "r" (dst) : "memory");
		src += 64;
		dst += 64;
		len -= 4;
	}
	while (len--) {
		asm("movntdqa (%0), %%xmm0\n"
		    "movaps %%xmm0, (%1)\n"
		    :: "r" (src), "r" (dst) : "memory");
		src += 16;
		dst += 16;
	}

	kernel_fpu_end();
}

 
static void __drm_memcpy_from_wc(void *dst, const void *src, unsigned long len)
{
	if (unlikely(((unsigned long)dst | (unsigned long)src | len) & 15))
		memcpy(dst, src, len);
	else if (likely(len))
		__memcpy_ntdqa(dst, src, len >> 4);
}

 
void drm_memcpy_from_wc(struct iosys_map *dst,
			const struct iosys_map *src,
			unsigned long len)
{
	if (WARN_ON(in_interrupt())) {
		memcpy_fallback(dst, src, len);
		return;
	}

	if (static_branch_likely(&has_movntdqa)) {
		__drm_memcpy_from_wc(dst->is_iomem ?
				     (void __force *)dst->vaddr_iomem :
				     dst->vaddr,
				     src->is_iomem ?
				     (void const __force *)src->vaddr_iomem :
				     src->vaddr,
				     len);
		return;
	}

	memcpy_fallback(dst, src, len);
}
EXPORT_SYMBOL(drm_memcpy_from_wc);

 
void drm_memcpy_init_early(void)
{
	 
	if (static_cpu_has(X86_FEATURE_XMM4_1) &&
	    !boot_cpu_has(X86_FEATURE_HYPERVISOR))
		static_branch_enable(&has_movntdqa);
}
#else
void drm_memcpy_from_wc(struct iosys_map *dst,
			const struct iosys_map *src,
			unsigned long len)
{
	WARN_ON(in_interrupt());

	memcpy_fallback(dst, src, len);
}
EXPORT_SYMBOL(drm_memcpy_from_wc);

void drm_memcpy_init_early(void)
{
}
#endif  
