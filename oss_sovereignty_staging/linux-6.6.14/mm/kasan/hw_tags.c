
 

#define pr_fmt(fmt) "kasan: " fmt

#include <linux/init.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/static_key.h>
#include <linux/string.h>
#include <linux/types.h>

#include "kasan.h"

enum kasan_arg {
	KASAN_ARG_DEFAULT,
	KASAN_ARG_OFF,
	KASAN_ARG_ON,
};

enum kasan_arg_mode {
	KASAN_ARG_MODE_DEFAULT,
	KASAN_ARG_MODE_SYNC,
	KASAN_ARG_MODE_ASYNC,
	KASAN_ARG_MODE_ASYMM,
};

enum kasan_arg_vmalloc {
	KASAN_ARG_VMALLOC_DEFAULT,
	KASAN_ARG_VMALLOC_OFF,
	KASAN_ARG_VMALLOC_ON,
};

static enum kasan_arg kasan_arg __ro_after_init;
static enum kasan_arg_mode kasan_arg_mode __ro_after_init;
static enum kasan_arg_vmalloc kasan_arg_vmalloc __initdata;

 
DEFINE_STATIC_KEY_FALSE(kasan_flag_enabled);
EXPORT_SYMBOL(kasan_flag_enabled);

 
enum kasan_mode kasan_mode __ro_after_init;
EXPORT_SYMBOL_GPL(kasan_mode);

 
DEFINE_STATIC_KEY_TRUE(kasan_flag_vmalloc);

#define PAGE_ALLOC_SAMPLE_DEFAULT	1
#define PAGE_ALLOC_SAMPLE_ORDER_DEFAULT	3

 
unsigned long kasan_page_alloc_sample = PAGE_ALLOC_SAMPLE_DEFAULT;

 
unsigned int kasan_page_alloc_sample_order = PAGE_ALLOC_SAMPLE_ORDER_DEFAULT;

DEFINE_PER_CPU(long, kasan_page_alloc_skip);

 
static int __init early_kasan_flag(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (!strcmp(arg, "off"))
		kasan_arg = KASAN_ARG_OFF;
	else if (!strcmp(arg, "on"))
		kasan_arg = KASAN_ARG_ON;
	else
		return -EINVAL;

	return 0;
}
early_param("kasan", early_kasan_flag);

 
static int __init early_kasan_mode(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (!strcmp(arg, "sync"))
		kasan_arg_mode = KASAN_ARG_MODE_SYNC;
	else if (!strcmp(arg, "async"))
		kasan_arg_mode = KASAN_ARG_MODE_ASYNC;
	else if (!strcmp(arg, "asymm"))
		kasan_arg_mode = KASAN_ARG_MODE_ASYMM;
	else
		return -EINVAL;

	return 0;
}
early_param("kasan.mode", early_kasan_mode);

 
static int __init early_kasan_flag_vmalloc(char *arg)
{
	if (!arg)
		return -EINVAL;

	if (!strcmp(arg, "off"))
		kasan_arg_vmalloc = KASAN_ARG_VMALLOC_OFF;
	else if (!strcmp(arg, "on"))
		kasan_arg_vmalloc = KASAN_ARG_VMALLOC_ON;
	else
		return -EINVAL;

	return 0;
}
early_param("kasan.vmalloc", early_kasan_flag_vmalloc);

static inline const char *kasan_mode_info(void)
{
	if (kasan_mode == KASAN_MODE_ASYNC)
		return "async";
	else if (kasan_mode == KASAN_MODE_ASYMM)
		return "asymm";
	else
		return "sync";
}

 
static int __init early_kasan_flag_page_alloc_sample(char *arg)
{
	int rv;

	if (!arg)
		return -EINVAL;

	rv = kstrtoul(arg, 0, &kasan_page_alloc_sample);
	if (rv)
		return rv;

	if (!kasan_page_alloc_sample || kasan_page_alloc_sample > LONG_MAX) {
		kasan_page_alloc_sample = PAGE_ALLOC_SAMPLE_DEFAULT;
		return -EINVAL;
	}

	return 0;
}
early_param("kasan.page_alloc.sample", early_kasan_flag_page_alloc_sample);

 
static int __init early_kasan_flag_page_alloc_sample_order(char *arg)
{
	int rv;

	if (!arg)
		return -EINVAL;

	rv = kstrtouint(arg, 0, &kasan_page_alloc_sample_order);
	if (rv)
		return rv;

	if (kasan_page_alloc_sample_order > INT_MAX) {
		kasan_page_alloc_sample_order = PAGE_ALLOC_SAMPLE_ORDER_DEFAULT;
		return -EINVAL;
	}

	return 0;
}
early_param("kasan.page_alloc.sample.order", early_kasan_flag_page_alloc_sample_order);

 
void kasan_init_hw_tags_cpu(void)
{
	 

	 
	if (kasan_arg == KASAN_ARG_OFF)
		return;

	 
	kasan_enable_hw_tags();
}

 
void __init kasan_init_hw_tags(void)
{
	 
	if (!system_supports_mte())
		return;

	 
	if (kasan_arg == KASAN_ARG_OFF)
		return;

	switch (kasan_arg_mode) {
	case KASAN_ARG_MODE_DEFAULT:
		 
		break;
	case KASAN_ARG_MODE_SYNC:
		kasan_mode = KASAN_MODE_SYNC;
		break;
	case KASAN_ARG_MODE_ASYNC:
		kasan_mode = KASAN_MODE_ASYNC;
		break;
	case KASAN_ARG_MODE_ASYMM:
		kasan_mode = KASAN_MODE_ASYMM;
		break;
	}

	switch (kasan_arg_vmalloc) {
	case KASAN_ARG_VMALLOC_DEFAULT:
		 
		break;
	case KASAN_ARG_VMALLOC_OFF:
		static_branch_disable(&kasan_flag_vmalloc);
		break;
	case KASAN_ARG_VMALLOC_ON:
		static_branch_enable(&kasan_flag_vmalloc);
		break;
	}

	kasan_init_tags();

	 
	static_branch_enable(&kasan_flag_enabled);

	pr_info("KernelAddressSanitizer initialized (hw-tags, mode=%s, vmalloc=%s, stacktrace=%s)\n",
		kasan_mode_info(),
		kasan_vmalloc_enabled() ? "on" : "off",
		kasan_stack_collection_enabled() ? "on" : "off");
}

#ifdef CONFIG_KASAN_VMALLOC

static void unpoison_vmalloc_pages(const void *addr, u8 tag)
{
	struct vm_struct *area;
	int i;

	 
	area = find_vm_area((void *)addr);
	if (WARN_ON(!area))
		return;

	for (i = 0; i < area->nr_pages; i++) {
		struct page *page = area->pages[i];

		page_kasan_tag_set(page, tag);
	}
}

static void init_vmalloc_pages(const void *start, unsigned long size)
{
	const void *addr;

	for (addr = start; addr < start + size; addr += PAGE_SIZE) {
		struct page *page = vmalloc_to_page(addr);

		clear_highpage_kasan_tagged(page);
	}
}

void *__kasan_unpoison_vmalloc(const void *start, unsigned long size,
				kasan_vmalloc_flags_t flags)
{
	u8 tag;
	unsigned long redzone_start, redzone_size;

	if (!kasan_vmalloc_enabled()) {
		if (flags & KASAN_VMALLOC_INIT)
			init_vmalloc_pages(start, size);
		return (void *)start;
	}

	 
	if (!(flags & KASAN_VMALLOC_VM_ALLOC)) {
		WARN_ON(flags & KASAN_VMALLOC_INIT);
		return (void *)start;
	}

	 
	if (!(flags & KASAN_VMALLOC_PROT_NORMAL)) {
		WARN_ON(flags & KASAN_VMALLOC_INIT);
		return (void *)start;
	}

	tag = kasan_random_tag();
	start = set_tag(start, tag);

	 
	kasan_unpoison(start, size, flags & KASAN_VMALLOC_INIT);

	 
	redzone_start = round_up((unsigned long)start + size,
				 KASAN_GRANULE_SIZE);
	redzone_size = round_up(redzone_start, PAGE_SIZE) - redzone_start;
	kasan_poison((void *)redzone_start, redzone_size, KASAN_TAG_INVALID,
		     flags & KASAN_VMALLOC_INIT);

	 
	unpoison_vmalloc_pages(start, tag);

	return (void *)start;
}

void __kasan_poison_vmalloc(const void *start, unsigned long size)
{
	 
}

#endif

void kasan_enable_hw_tags(void)
{
	if (kasan_arg_mode == KASAN_ARG_MODE_ASYNC)
		hw_enable_tag_checks_async();
	else if (kasan_arg_mode == KASAN_ARG_MODE_ASYMM)
		hw_enable_tag_checks_asymm();
	else
		hw_enable_tag_checks_sync();
}

#if IS_ENABLED(CONFIG_KASAN_KUNIT_TEST)

EXPORT_SYMBOL_GPL(kasan_enable_hw_tags);

void kasan_force_async_fault(void)
{
	hw_force_async_tag_fault();
}
EXPORT_SYMBOL_GPL(kasan_force_async_fault);

#endif
