#ifndef I915_SCATTERLIST_H
#define I915_SCATTERLIST_H
#include <linux/pfn.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <xen/xen.h>
#include "i915_gem.h"
struct drm_mm_node;
struct ttm_resource;
static __always_inline struct sgt_iter {
	struct scatterlist *sgp;
	union {
		unsigned long pfn;
		dma_addr_t dma;
	};
	unsigned int curr;
	unsigned int max;
} __sgt_iter(struct scatterlist *sgl, bool dma) {
	struct sgt_iter s = { .sgp = sgl };
	if (dma && s.sgp && sg_dma_len(s.sgp) == 0) {
		s.sgp = NULL;
	} else if (s.sgp) {
		s.max = s.curr = s.sgp->offset;
		if (dma) {
			s.dma = sg_dma_address(s.sgp);
			s.max += sg_dma_len(s.sgp);
		} else {
			s.pfn = page_to_pfn(sg_page(s.sgp));
			s.max += s.sgp->length;
		}
	}
	return s;
}
static inline int __sg_page_count(const struct scatterlist *sg)
{
	return sg->length >> PAGE_SHIFT;
}
static inline int __sg_dma_page_count(const struct scatterlist *sg)
{
	return sg_dma_len(sg) >> PAGE_SHIFT;
}
static inline struct scatterlist *____sg_next(struct scatterlist *sg)
{
	++sg;
	if (unlikely(sg_is_chain(sg)))
		sg = sg_chain_ptr(sg);
	return sg;
}
static inline struct scatterlist *__sg_next(struct scatterlist *sg)
{
	return sg_is_last(sg) ? NULL : ____sg_next(sg);
}
#define __for_each_sgt_daddr(__dp, __iter, __sgt, __step)		\
	for ((__iter) = __sgt_iter((__sgt)->sgl, true);			\
	     ((__dp) = (__iter).dma + (__iter).curr), (__iter).sgp;	\
	     (((__iter).curr += (__step)) >= (__iter).max) ?		\
	     (__iter) = __sgt_iter(__sg_next((__iter).sgp), true), 0 : 0)
#define for_each_sgt_page(__pp, __iter, __sgt)				\
	for ((__iter) = __sgt_iter((__sgt)->sgl, false);		\
	     ((__pp) = (__iter).pfn == 0 ? NULL :			\
	      pfn_to_page((__iter).pfn + ((__iter).curr >> PAGE_SHIFT))); \
	     (((__iter).curr += PAGE_SIZE) >= (__iter).max) ?		\
	     (__iter) = __sgt_iter(__sg_next((__iter).sgp), false), 0 : 0)
static inline unsigned int i915_sg_dma_sizes(struct scatterlist *sg)
{
	unsigned int page_sizes;
	page_sizes = 0;
	while (sg && sg_dma_len(sg)) {
		GEM_BUG_ON(sg->offset);
		GEM_BUG_ON(!IS_ALIGNED(sg_dma_len(sg), PAGE_SIZE));
		page_sizes |= sg_dma_len(sg);
		sg = __sg_next(sg);
	}
	return page_sizes;
}
static inline unsigned int i915_sg_segment_size(struct device *dev)
{
	size_t max = min_t(size_t, UINT_MAX, dma_max_mapping_size(dev));
	if (xen_pv_domain())
		max = PAGE_SIZE;
	return round_down(max, PAGE_SIZE);
}
bool i915_sg_trim(struct sg_table *orig_st);
struct i915_refct_sgt_ops {
	void (*release)(struct kref *ref);
};
struct i915_refct_sgt {
	struct kref kref;
	struct sg_table table;
	size_t size;
	const struct i915_refct_sgt_ops *ops;
};
static inline void i915_refct_sgt_put(struct i915_refct_sgt *rsgt)
{
	if (rsgt)
		kref_put(&rsgt->kref, rsgt->ops->release);
}
static inline struct i915_refct_sgt *
i915_refct_sgt_get(struct i915_refct_sgt *rsgt)
{
	kref_get(&rsgt->kref);
	return rsgt;
}
static inline void __i915_refct_sgt_init(struct i915_refct_sgt *rsgt,
					 size_t size,
					 const struct i915_refct_sgt_ops *ops)
{
	kref_init(&rsgt->kref);
	rsgt->table.sgl = NULL;
	rsgt->size = size;
	rsgt->ops = ops;
}
void i915_refct_sgt_init(struct i915_refct_sgt *rsgt, size_t size);
struct i915_refct_sgt *i915_rsgt_from_mm_node(const struct drm_mm_node *node,
					      u64 region_start,
					      u32 page_alignment);
struct i915_refct_sgt *i915_rsgt_from_buddy_resource(struct ttm_resource *res,
						     u64 region_start,
						     u32 page_alignment);
#endif
