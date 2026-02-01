 

#include <linux/kernel.h>
#include <asm/fpu/api.h>

#include "i915_memcpy.h"

#if IS_ENABLED(CONFIG_DRM_I915_DEBUG)
#define CI_BUG_ON(expr) BUG_ON(expr)
#else
#define CI_BUG_ON(expr) BUILD_BUG_ON_INVALID(expr)
#endif

static DEFINE_STATIC_KEY_FALSE(has_movntdqa);

static void __memcpy_ntdqa(void *dst, const void *src, unsigned long len)
{
	kernel_fpu_begin();

	while (len >= 4) {
		asm("movntdqa   (%0), %%xmm0\n"
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

static void __memcpy_ntdqu(void *dst, const void *src, unsigned long len)
{
	kernel_fpu_begin();

	while (len >= 4) {
		asm("movntdqa   (%0), %%xmm0\n"
		    "movntdqa 16(%0), %%xmm1\n"
		    "movntdqa 32(%0), %%xmm2\n"
		    "movntdqa 48(%0), %%xmm3\n"
		    "movups %%xmm0,   (%1)\n"
		    "movups %%xmm1, 16(%1)\n"
		    "movups %%xmm2, 32(%1)\n"
		    "movups %%xmm3, 48(%1)\n"
		    :: "r" (src), "r" (dst) : "memory");
		src += 64;
		dst += 64;
		len -= 4;
	}
	while (len--) {
		asm("movntdqa (%0), %%xmm0\n"
		    "movups %%xmm0, (%1)\n"
		    :: "r" (src), "r" (dst) : "memory");
		src += 16;
		dst += 16;
	}

	kernel_fpu_end();
}

 
bool i915_memcpy_from_wc(void *dst, const void *src, unsigned long len)
{
	if (unlikely(((unsigned long)dst | (unsigned long)src | len) & 15))
		return false;

	if (static_branch_likely(&has_movntdqa)) {
		if (likely(len))
			__memcpy_ntdqa(dst, src, len >> 4);
		return true;
	}

	return false;
}

 
void i915_unaligned_memcpy_from_wc(void *dst, const void *src, unsigned long len)
{
	unsigned long addr;

	CI_BUG_ON(!i915_has_memcpy_from_wc());

	addr = (unsigned long)src;
	if (!IS_ALIGNED(addr, 16)) {
		unsigned long x = min(ALIGN(addr, 16) - addr, len);

		memcpy(dst, src, x);

		len -= x;
		dst += x;
		src += x;
	}

	if (likely(len))
		__memcpy_ntdqu(dst, src, DIV_ROUND_UP(len, 16));
}

void i915_memcpy_init_early(struct drm_i915_private *dev_priv)
{
	 
	if (static_cpu_has(X86_FEATURE_XMM4_1) &&
	    !boot_cpu_has(X86_FEATURE_HYPERVISOR))
		static_branch_enable(&has_movntdqa);
}
