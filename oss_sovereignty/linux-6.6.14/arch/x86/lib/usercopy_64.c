
 
#include <linux/export.h>
#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/libnvdimm.h>

 

#ifdef CONFIG_ARCH_HAS_UACCESS_FLUSHCACHE
 
static void clean_cache_range(void *addr, size_t size)
{
	u16 x86_clflush_size = boot_cpu_data.x86_clflush_size;
	unsigned long clflush_mask = x86_clflush_size - 1;
	void *vend = addr + size;
	void *p;

	for (p = (void *)((unsigned long)addr & ~clflush_mask);
	     p < vend; p += x86_clflush_size)
		clwb(p);
}

void arch_wb_cache_pmem(void *addr, size_t size)
{
	clean_cache_range(addr, size);
}
EXPORT_SYMBOL_GPL(arch_wb_cache_pmem);

long __copy_user_flushcache(void *dst, const void __user *src, unsigned size)
{
	unsigned long flushed, dest = (unsigned long) dst;
	long rc;

	stac();
	rc = __copy_user_nocache(dst, src, size);
	clac();

	 
	if (size < 8) {
		if (!IS_ALIGNED(dest, 4) || size != 4)
			clean_cache_range(dst, size);
	} else {
		if (!IS_ALIGNED(dest, 8)) {
			dest = ALIGN(dest, boot_cpu_data.x86_clflush_size);
			clean_cache_range(dst, 1);
		}

		flushed = dest - (unsigned long) dst;
		if (size > flushed && !IS_ALIGNED(size - flushed, 8))
			clean_cache_range(dst + size - 1, 1);
	}

	return rc;
}

void __memcpy_flushcache(void *_dst, const void *_src, size_t size)
{
	unsigned long dest = (unsigned long) _dst;
	unsigned long source = (unsigned long) _src;

	 
	if (!IS_ALIGNED(dest, 8)) {
		size_t len = min_t(size_t, size, ALIGN(dest, 8) - dest);

		memcpy((void *) dest, (void *) source, len);
		clean_cache_range((void *) dest, len);
		dest += len;
		source += len;
		size -= len;
		if (!size)
			return;
	}

	 
	while (size >= 32) {
		asm("movq    (%0), %%r8\n"
		    "movq   8(%0), %%r9\n"
		    "movq  16(%0), %%r10\n"
		    "movq  24(%0), %%r11\n"
		    "movnti  %%r8,   (%1)\n"
		    "movnti  %%r9,  8(%1)\n"
		    "movnti %%r10, 16(%1)\n"
		    "movnti %%r11, 24(%1)\n"
		    :: "r" (source), "r" (dest)
		    : "memory", "r8", "r9", "r10", "r11");
		dest += 32;
		source += 32;
		size -= 32;
	}

	 
	while (size >= 8) {
		asm("movq    (%0), %%r8\n"
		    "movnti  %%r8,   (%1)\n"
		    :: "r" (source), "r" (dest)
		    : "memory", "r8");
		dest += 8;
		source += 8;
		size -= 8;
	}

	 
	while (size >= 4) {
		asm("movl    (%0), %%r8d\n"
		    "movnti  %%r8d,   (%1)\n"
		    :: "r" (source), "r" (dest)
		    : "memory", "r8");
		dest += 4;
		source += 4;
		size -= 4;
	}

	 
	if (size) {
		memcpy((void *) dest, (void *) source, size);
		clean_cache_range((void *) dest, size);
	}
}
EXPORT_SYMBOL_GPL(__memcpy_flushcache);
#endif
