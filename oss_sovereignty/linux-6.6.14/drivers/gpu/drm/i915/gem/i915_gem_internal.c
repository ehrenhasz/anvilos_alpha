 

#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "i915_drv.h"
#include "i915_gem.h"
#include "i915_gem_internal.h"
#include "i915_gem_object.h"
#include "i915_scatterlist.h"
#include "i915_utils.h"

#define QUIET (__GFP_NORETRY | __GFP_NOWARN)
#define MAYFAIL (__GFP_RETRY_MAYFAIL | __GFP_NOWARN)

static void internal_free_pages(struct sg_table *st)
{
	struct scatterlist *sg;

	for (sg = st->sgl; sg; sg = __sg_next(sg)) {
		if (sg_page(sg))
			__free_pages(sg_page(sg), get_order(sg->length));
	}

	sg_free_table(st);
	kfree(st);
}

static int i915_gem_object_get_pages_internal(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct sg_table *st;
	struct scatterlist *sg;
	unsigned int npages;  
	int max_order = MAX_ORDER;
	unsigned int max_segment;
	gfp_t gfp;

	if (overflows_type(obj->base.size >> PAGE_SHIFT, npages))
		return -E2BIG;

	npages = obj->base.size >> PAGE_SHIFT;
	max_segment = i915_sg_segment_size(i915->drm.dev) >> PAGE_SHIFT;
	max_order = min(max_order, get_order(max_segment));

	gfp = GFP_KERNEL | __GFP_HIGHMEM | __GFP_RECLAIMABLE;
	if (IS_I965GM(i915) || IS_I965G(i915)) {
		 
		gfp &= ~__GFP_HIGHMEM;
		gfp |= __GFP_DMA32;
	}

create_st:
	st = kmalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return -ENOMEM;

	if (sg_alloc_table(st, npages, GFP_KERNEL)) {
		kfree(st);
		return -ENOMEM;
	}

	sg = st->sgl;
	st->nents = 0;

	do {
		int order = min(fls(npages) - 1, max_order);
		struct page *page;

		do {
			page = alloc_pages(gfp | (order ? QUIET : MAYFAIL),
					   order);
			if (page)
				break;
			if (!order--)
				goto err;

			 
			max_order = order;
		} while (1);

		sg_set_page(sg, page, PAGE_SIZE << order, 0);
		st->nents++;

		npages -= 1 << order;
		if (!npages) {
			sg_mark_end(sg);
			break;
		}

		sg = __sg_next(sg);
	} while (1);

	if (i915_gem_gtt_prepare_pages(obj, st)) {
		 
		if (get_order(st->sgl->length)) {
			internal_free_pages(st);
			max_order = 0;
			goto create_st;
		}
		goto err;
	}

	__i915_gem_object_set_pages(obj, st);

	return 0;

err:
	sg_set_page(sg, NULL, 0, 0);
	sg_mark_end(sg);
	internal_free_pages(st);

	return -ENOMEM;
}

static void i915_gem_object_put_pages_internal(struct drm_i915_gem_object *obj,
					       struct sg_table *pages)
{
	i915_gem_gtt_finish_pages(obj, pages);
	internal_free_pages(pages);

	obj->mm.dirty = false;

	__start_cpu_write(obj);
}

static const struct drm_i915_gem_object_ops i915_gem_object_internal_ops = {
	.name = "i915_gem_object_internal",
	.flags = I915_GEM_OBJECT_IS_SHRINKABLE,
	.get_pages = i915_gem_object_get_pages_internal,
	.put_pages = i915_gem_object_put_pages_internal,
};

struct drm_i915_gem_object *
__i915_gem_object_create_internal(struct drm_i915_private *i915,
				  const struct drm_i915_gem_object_ops *ops,
				  phys_addr_t size)
{
	static struct lock_class_key lock_class;
	struct drm_i915_gem_object *obj;
	unsigned int cache_level;

	GEM_BUG_ON(!size);
	GEM_BUG_ON(!IS_ALIGNED(size, PAGE_SIZE));

	if (overflows_type(size, obj->base.size))
		return ERR_PTR(-E2BIG);

	obj = i915_gem_object_alloc();
	if (!obj)
		return ERR_PTR(-ENOMEM);

	drm_gem_private_object_init(&i915->drm, &obj->base, size);
	i915_gem_object_init(obj, ops, &lock_class, 0);
	obj->mem_flags |= I915_BO_FLAG_STRUCT_PAGE;

	 
	i915_gem_object_set_volatile(obj);

	obj->read_domains = I915_GEM_DOMAIN_CPU;
	obj->write_domain = I915_GEM_DOMAIN_CPU;

	cache_level = HAS_LLC(i915) ? I915_CACHE_LLC : I915_CACHE_NONE;
	i915_gem_object_set_cache_coherency(obj, cache_level);

	return obj;
}

 
struct drm_i915_gem_object *
i915_gem_object_create_internal(struct drm_i915_private *i915,
				phys_addr_t size)
{
	return __i915_gem_object_create_internal(i915, &i915_gem_object_internal_ops, size);
}
