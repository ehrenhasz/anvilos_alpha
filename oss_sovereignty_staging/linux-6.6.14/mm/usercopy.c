
 
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/kstrtox.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/task_stack.h>
#include <linux/thread_info.h>
#include <linux/vmalloc.h>
#include <linux/atomic.h>
#include <linux/jump_label.h>
#include <asm/sections.h>
#include "slab.h"

 
static noinline int check_stack_object(const void *obj, unsigned long len)
{
	const void * const stack = task_stack_page(current);
	const void * const stackend = stack + THREAD_SIZE;
	int ret;

	 
	if (obj + len <= stack || stackend <= obj)
		return NOT_STACK;

	 
	if (obj < stack || stackend < obj + len)
		return BAD_STACK;

	 
	ret = arch_within_stack_frames(stack, stackend, obj, len);
	if (ret)
		return ret;

	 
#ifdef CONFIG_ARCH_HAS_CURRENT_STACK_POINTER
	if (IS_ENABLED(CONFIG_STACK_GROWSUP)) {
		if ((void *)current_stack_pointer < obj + len)
			return BAD_STACK;
	} else {
		if (obj < (void *)current_stack_pointer)
			return BAD_STACK;
	}
#endif

	return GOOD_STACK;
}

 
void __noreturn usercopy_abort(const char *name, const char *detail,
			       bool to_user, unsigned long offset,
			       unsigned long len)
{
	pr_emerg("Kernel memory %s attempt detected %s %s%s%s%s (offset %lu, size %lu)!\n",
		 to_user ? "exposure" : "overwrite",
		 to_user ? "from" : "to",
		 name ? : "unknown?!",
		 detail ? " '" : "", detail ? : "", detail ? "'" : "",
		 offset, len);

	 
	BUG();
}

 
static bool overlaps(const unsigned long ptr, unsigned long n,
		     unsigned long low, unsigned long high)
{
	const unsigned long check_low = ptr;
	unsigned long check_high = check_low + n;

	 
	if (check_low >= high || check_high <= low)
		return false;

	return true;
}

 
static inline void check_kernel_text_object(const unsigned long ptr,
					    unsigned long n, bool to_user)
{
	unsigned long textlow = (unsigned long)_stext;
	unsigned long texthigh = (unsigned long)_etext;
	unsigned long textlow_linear, texthigh_linear;

	if (overlaps(ptr, n, textlow, texthigh))
		usercopy_abort("kernel text", NULL, to_user, ptr - textlow, n);

	 
	textlow_linear = (unsigned long)lm_alias(textlow);
	 
	if (textlow_linear == textlow)
		return;

	 
	texthigh_linear = (unsigned long)lm_alias(texthigh);
	if (overlaps(ptr, n, textlow_linear, texthigh_linear))
		usercopy_abort("linear kernel text", NULL, to_user,
			       ptr - textlow_linear, n);
}

static inline void check_bogus_address(const unsigned long ptr, unsigned long n,
				       bool to_user)
{
	 
	if (ptr + (n - 1) < ptr)
		usercopy_abort("wrapped address", NULL, to_user, 0, ptr + n);

	 
	if (ZERO_OR_NULL_PTR(ptr))
		usercopy_abort("null address", NULL, to_user, ptr, n);
}

static inline void check_heap_object(const void *ptr, unsigned long n,
				     bool to_user)
{
	unsigned long addr = (unsigned long)ptr;
	unsigned long offset;
	struct folio *folio;

	if (is_kmap_addr(ptr)) {
		offset = offset_in_page(ptr);
		if (n > PAGE_SIZE - offset)
			usercopy_abort("kmap", NULL, to_user, offset, n);
		return;
	}

	if (is_vmalloc_addr(ptr) && !pagefault_disabled()) {
		struct vmap_area *area = find_vmap_area(addr);

		if (!area)
			usercopy_abort("vmalloc", "no area", to_user, 0, n);

		if (n > area->va_end - addr) {
			offset = addr - area->va_start;
			usercopy_abort("vmalloc", NULL, to_user, offset, n);
		}
		return;
	}

	if (!virt_addr_valid(ptr))
		return;

	folio = virt_to_folio(ptr);

	if (folio_test_slab(folio)) {
		 
		__check_heap_object(ptr, n, folio_slab(folio), to_user);
	} else if (folio_test_large(folio)) {
		offset = ptr - folio_address(folio);
		if (n > folio_size(folio) - offset)
			usercopy_abort("page alloc", NULL, to_user, offset, n);
	}
}

static DEFINE_STATIC_KEY_FALSE_RO(bypass_usercopy_checks);

 
void __check_object_size(const void *ptr, unsigned long n, bool to_user)
{
	if (static_branch_unlikely(&bypass_usercopy_checks))
		return;

	 
	if (!n)
		return;

	 
	check_bogus_address((const unsigned long)ptr, n, to_user);

	 
	switch (check_stack_object(ptr, n)) {
	case NOT_STACK:
		 
		break;
	case GOOD_FRAME:
	case GOOD_STACK:
		 
		return;
	default:
		usercopy_abort("process stack", NULL, to_user,
#ifdef CONFIG_ARCH_HAS_CURRENT_STACK_POINTER
			IS_ENABLED(CONFIG_STACK_GROWSUP) ?
				ptr - (void *)current_stack_pointer :
				(void *)current_stack_pointer - ptr,
#else
			0,
#endif
			n);
	}

	 
	check_heap_object(ptr, n, to_user);

	 
	check_kernel_text_object((const unsigned long)ptr, n, to_user);
}
EXPORT_SYMBOL(__check_object_size);

static bool enable_checks __initdata = true;

static int __init parse_hardened_usercopy(char *str)
{
	if (kstrtobool(str, &enable_checks))
		pr_warn("Invalid option string for hardened_usercopy: '%s'\n",
			str);
	return 1;
}

__setup("hardened_usercopy=", parse_hardened_usercopy);

static int __init set_hardened_usercopy(void)
{
	if (enable_checks == false)
		static_branch_enable(&bypass_usercopy_checks);
	return 1;
}

late_initcall(set_hardened_usercopy);
