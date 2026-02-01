
 

#include <linux/export.h>
#include <linux/init.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/linkage.h>
#include <linux/memblock.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/bug.h>

#include "kasan.h"
#include "../slab.h"

struct slab *kasan_addr_to_slab(const void *addr)
{
	if (virt_addr_valid(addr))
		return virt_to_slab(addr);
	return NULL;
}

depot_stack_handle_t kasan_save_stack(gfp_t flags, bool can_alloc)
{
	unsigned long entries[KASAN_STACK_DEPTH];
	unsigned int nr_entries;

	nr_entries = stack_trace_save(entries, ARRAY_SIZE(entries), 0);
	return __stack_depot_save(entries, nr_entries, flags, can_alloc);
}

void kasan_set_track(struct kasan_track *track, gfp_t flags)
{
	track->pid = current->pid;
	track->stack = kasan_save_stack(flags, true);
}

#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
void kasan_enable_current(void)
{
	current->kasan_depth++;
}
EXPORT_SYMBOL(kasan_enable_current);

void kasan_disable_current(void)
{
	current->kasan_depth--;
}
EXPORT_SYMBOL(kasan_disable_current);

#endif  

void __kasan_unpoison_range(const void *address, size_t size)
{
	kasan_unpoison(address, size, false);
}

#ifdef CONFIG_KASAN_STACK
 
void kasan_unpoison_task_stack(struct task_struct *task)
{
	void *base = task_stack_page(task);

	kasan_unpoison(base, THREAD_SIZE, false);
}

 
asmlinkage void kasan_unpoison_task_stack_below(const void *watermark)
{
	 
	void *base = (void *)((unsigned long)watermark & ~(THREAD_SIZE - 1));

	kasan_unpoison(base, watermark - base, false);
}
#endif  

bool __kasan_unpoison_pages(struct page *page, unsigned int order, bool init)
{
	u8 tag;
	unsigned long i;

	if (unlikely(PageHighMem(page)))
		return false;

	if (!kasan_sample_page_alloc(order))
		return false;

	tag = kasan_random_tag();
	kasan_unpoison(set_tag(page_address(page), tag),
		       PAGE_SIZE << order, init);
	for (i = 0; i < (1 << order); i++)
		page_kasan_tag_set(page + i, tag);

	return true;
}

void __kasan_poison_pages(struct page *page, unsigned int order, bool init)
{
	if (likely(!PageHighMem(page)))
		kasan_poison(page_address(page), PAGE_SIZE << order,
			     KASAN_PAGE_FREE, init);
}

void __kasan_poison_slab(struct slab *slab)
{
	struct page *page = slab_page(slab);
	unsigned long i;

	for (i = 0; i < compound_nr(page); i++)
		page_kasan_tag_reset(page + i);
	kasan_poison(page_address(page), page_size(page),
		     KASAN_SLAB_REDZONE, false);
}

void __kasan_unpoison_object_data(struct kmem_cache *cache, void *object)
{
	kasan_unpoison(object, cache->object_size, false);
}

void __kasan_poison_object_data(struct kmem_cache *cache, void *object)
{
	kasan_poison(object, round_up(cache->object_size, KASAN_GRANULE_SIZE),
			KASAN_SLAB_REDZONE, false);
}

 
static inline u8 assign_tag(struct kmem_cache *cache,
					const void *object, bool init)
{
	if (IS_ENABLED(CONFIG_KASAN_GENERIC))
		return 0xff;

	 
	if (!cache->ctor && !(cache->flags & SLAB_TYPESAFE_BY_RCU))
		return init ? KASAN_TAG_KERNEL : kasan_random_tag();

	 
#ifdef CONFIG_SLAB
	 
	return (u8)obj_to_index(cache, virt_to_slab(object), (void *)object);
#else
	 
	return init ? kasan_random_tag() : get_tag(object);
#endif
}

void * __must_check __kasan_init_slab_obj(struct kmem_cache *cache,
						const void *object)
{
	 
	if (kasan_requires_meta())
		kasan_init_object_meta(cache, object);

	 
	object = set_tag(object, assign_tag(cache, object, true));

	return (void *)object;
}

static inline bool ____kasan_slab_free(struct kmem_cache *cache, void *object,
				unsigned long ip, bool quarantine, bool init)
{
	void *tagged_object;

	if (!kasan_arch_is_ready())
		return false;

	tagged_object = object;
	object = kasan_reset_tag(object);

	if (is_kfence_address(object))
		return false;

	if (unlikely(nearest_obj(cache, virt_to_slab(object), object) !=
	    object)) {
		kasan_report_invalid_free(tagged_object, ip, KASAN_REPORT_INVALID_FREE);
		return true;
	}

	 
	if (unlikely(cache->flags & SLAB_TYPESAFE_BY_RCU))
		return false;

	if (!kasan_byte_accessible(tagged_object)) {
		kasan_report_invalid_free(tagged_object, ip, KASAN_REPORT_DOUBLE_FREE);
		return true;
	}

	kasan_poison(object, round_up(cache->object_size, KASAN_GRANULE_SIZE),
			KASAN_SLAB_FREE, init);

	if ((IS_ENABLED(CONFIG_KASAN_GENERIC) && !quarantine))
		return false;

	if (kasan_stack_collection_enabled())
		kasan_save_free_info(cache, tagged_object);

	return kasan_quarantine_put(cache, object);
}

bool __kasan_slab_free(struct kmem_cache *cache, void *object,
				unsigned long ip, bool init)
{
	return ____kasan_slab_free(cache, object, ip, true, init);
}

static inline bool ____kasan_kfree_large(void *ptr, unsigned long ip)
{
	if (!kasan_arch_is_ready())
		return false;

	if (ptr != page_address(virt_to_head_page(ptr))) {
		kasan_report_invalid_free(ptr, ip, KASAN_REPORT_INVALID_FREE);
		return true;
	}

	if (!kasan_byte_accessible(ptr)) {
		kasan_report_invalid_free(ptr, ip, KASAN_REPORT_DOUBLE_FREE);
		return true;
	}

	 

	return false;
}

void __kasan_kfree_large(void *ptr, unsigned long ip)
{
	____kasan_kfree_large(ptr, ip);
}

void __kasan_slab_free_mempool(void *ptr, unsigned long ip)
{
	struct folio *folio;

	folio = virt_to_folio(ptr);

	 
	if (unlikely(!folio_test_slab(folio))) {
		if (____kasan_kfree_large(ptr, ip))
			return;
		kasan_poison(ptr, folio_size(folio), KASAN_PAGE_FREE, false);
	} else {
		struct slab *slab = folio_slab(folio);

		____kasan_slab_free(slab->slab_cache, ptr, ip, false, false);
	}
}

void * __must_check __kasan_slab_alloc(struct kmem_cache *cache,
					void *object, gfp_t flags, bool init)
{
	u8 tag;
	void *tagged_object;

	if (gfpflags_allow_blocking(flags))
		kasan_quarantine_reduce();

	if (unlikely(object == NULL))
		return NULL;

	if (is_kfence_address(object))
		return (void *)object;

	 
	tag = assign_tag(cache, object, false);
	tagged_object = set_tag(object, tag);

	 
	kasan_unpoison(tagged_object, cache->object_size, init);

	 
	if (kasan_stack_collection_enabled() && !is_kmalloc_cache(cache))
		kasan_save_alloc_info(cache, tagged_object, flags);

	return tagged_object;
}

static inline void *____kasan_kmalloc(struct kmem_cache *cache,
				const void *object, size_t size, gfp_t flags)
{
	unsigned long redzone_start;
	unsigned long redzone_end;

	if (gfpflags_allow_blocking(flags))
		kasan_quarantine_reduce();

	if (unlikely(object == NULL))
		return NULL;

	if (is_kfence_address(kasan_reset_tag(object)))
		return (void *)object;

	 

	 
	if (IS_ENABLED(CONFIG_KASAN_GENERIC))
		kasan_poison_last_granule((void *)object, size);

	 
	redzone_start = round_up((unsigned long)(object + size),
				KASAN_GRANULE_SIZE);
	redzone_end = round_up((unsigned long)(object + cache->object_size),
				KASAN_GRANULE_SIZE);
	kasan_poison((void *)redzone_start, redzone_end - redzone_start,
			   KASAN_SLAB_REDZONE, false);

	 
	if (kasan_stack_collection_enabled() && is_kmalloc_cache(cache))
		kasan_save_alloc_info(cache, (void *)object, flags);

	 
	return (void *)object;
}

void * __must_check __kasan_kmalloc(struct kmem_cache *cache, const void *object,
					size_t size, gfp_t flags)
{
	return ____kasan_kmalloc(cache, object, size, flags);
}
EXPORT_SYMBOL(__kasan_kmalloc);

void * __must_check __kasan_kmalloc_large(const void *ptr, size_t size,
						gfp_t flags)
{
	unsigned long redzone_start;
	unsigned long redzone_end;

	if (gfpflags_allow_blocking(flags))
		kasan_quarantine_reduce();

	if (unlikely(ptr == NULL))
		return NULL;

	 

	 
	if (IS_ENABLED(CONFIG_KASAN_GENERIC))
		kasan_poison_last_granule(ptr, size);

	 
	redzone_start = round_up((unsigned long)(ptr + size),
				KASAN_GRANULE_SIZE);
	redzone_end = (unsigned long)ptr + page_size(virt_to_page(ptr));
	kasan_poison((void *)redzone_start, redzone_end - redzone_start,
		     KASAN_PAGE_REDZONE, false);

	return (void *)ptr;
}

void * __must_check __kasan_krealloc(const void *object, size_t size, gfp_t flags)
{
	struct slab *slab;

	if (unlikely(object == ZERO_SIZE_PTR))
		return (void *)object;

	 
	kasan_unpoison(object, size, false);

	slab = virt_to_slab(object);

	 
	if (unlikely(!slab))
		return __kasan_kmalloc_large(object, size, flags);
	else
		return ____kasan_kmalloc(slab->slab_cache, object, size, flags);
}

bool __kasan_check_byte(const void *address, unsigned long ip)
{
	if (!kasan_byte_accessible(address)) {
		kasan_report(address, 1, false, ip);
		return false;
	}
	return true;
}
