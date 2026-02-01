
 

#include "vmwgfx_drv.h"
#include <linux/highmem.h>

 
#define VMW_FIND_FIRST_DIFF(_type)			 \
static size_t vmw_find_first_diff_ ## _type		 \
	(const _type * dst, const _type * src, size_t size)\
{							 \
	size_t i;					 \
							 \
	for (i = 0; i < size; i += sizeof(_type)) {	 \
		if (*dst++ != *src++)			 \
			break;				 \
	}						 \
							 \
	return i;					 \
}


 
#define VMW_FIND_LAST_DIFF(_type)					\
static ssize_t vmw_find_last_diff_ ## _type(				\
	const _type * dst, const _type * src, size_t size)		\
{									\
	while (size) {							\
		if (*--dst != *--src)					\
			break;						\
									\
		size -= sizeof(_type);					\
	}								\
	return size;							\
}


 
VMW_FIND_FIRST_DIFF(u8);
VMW_FIND_LAST_DIFF(u8);

VMW_FIND_FIRST_DIFF(u16);
VMW_FIND_LAST_DIFF(u16);

VMW_FIND_FIRST_DIFF(u32);
VMW_FIND_LAST_DIFF(u32);

#ifdef CONFIG_64BIT
VMW_FIND_FIRST_DIFF(u64);
VMW_FIND_LAST_DIFF(u64);
#endif


 
#define SPILL(_var, _type) ((unsigned long) _var & (sizeof(_type) - 1))


 
#define VMW_TRY_FIND_FIRST_DIFF(_type)					\
do {									\
	unsigned int spill = SPILL(dst, _type);				\
	size_t diff_offs;						\
									\
	if (spill && spill == SPILL(src, _type) &&			\
	    sizeof(_type) - spill <= size) {				\
		spill = sizeof(_type) - spill;				\
		diff_offs = vmw_find_first_diff_u8(dst, src, spill);	\
		if (diff_offs < spill)					\
			return round_down(offset + diff_offs, granularity); \
									\
		dst += spill;						\
		src += spill;						\
		size -= spill;						\
		offset += spill;					\
		spill = 0;						\
	}								\
	if (!spill && !SPILL(src, _type)) {				\
		size_t to_copy = size &	 ~(sizeof(_type) - 1);		\
									\
		diff_offs = vmw_find_first_diff_ ## _type		\
			((_type *) dst, (_type *) src, to_copy);	\
		if (diff_offs >= size || granularity == sizeof(_type))	\
			return (offset + diff_offs);			\
									\
		dst += diff_offs;					\
		src += diff_offs;					\
		size -= diff_offs;					\
		offset += diff_offs;					\
	}								\
} while (0)								\


 
static size_t vmw_find_first_diff(const u8 *dst, const u8 *src, size_t size,
				  size_t granularity)
{
	size_t offset = 0;

	 
#ifdef CONFIG_64BIT
	VMW_TRY_FIND_FIRST_DIFF(u64);
#endif
	VMW_TRY_FIND_FIRST_DIFF(u32);
	VMW_TRY_FIND_FIRST_DIFF(u16);

	return round_down(offset + vmw_find_first_diff_u8(dst, src, size),
			  granularity);
}


 
#define VMW_TRY_FIND_LAST_DIFF(_type)					\
do {									\
	unsigned int spill = SPILL(dst, _type);				\
	ssize_t location;						\
	ssize_t diff_offs;						\
									\
	if (spill && spill <= size && spill == SPILL(src, _type)) {	\
		diff_offs = vmw_find_last_diff_u8(dst, src, spill);	\
		if (diff_offs) {					\
			location = size - spill + diff_offs - 1;	\
			return round_down(location, granularity);	\
		}							\
									\
		dst -= spill;						\
		src -= spill;						\
		size -= spill;						\
		spill = 0;						\
	}								\
	if (!spill && !SPILL(src, _type)) {				\
		size_t to_copy = round_down(size, sizeof(_type));	\
									\
		diff_offs = vmw_find_last_diff_ ## _type		\
			((_type *) dst, (_type *) src, to_copy);	\
		location = size - to_copy + diff_offs - sizeof(_type);	\
		if (location < 0 || granularity == sizeof(_type))	\
			return location;				\
									\
		dst -= to_copy - diff_offs;				\
		src -= to_copy - diff_offs;				\
		size -= to_copy - diff_offs;				\
	}								\
} while (0)


 
static ssize_t vmw_find_last_diff(const u8 *dst, const u8 *src, size_t size,
				  size_t granularity)
{
	dst += size;
	src += size;

#ifdef CONFIG_64BIT
	VMW_TRY_FIND_LAST_DIFF(u64);
#endif
	VMW_TRY_FIND_LAST_DIFF(u32);
	VMW_TRY_FIND_LAST_DIFF(u16);

	return round_down(vmw_find_last_diff_u8(dst, src, size) - 1,
			  granularity);
}


 
void vmw_memcpy(struct vmw_diff_cpy *diff, u8 *dest, const u8 *src, size_t n)
{
	memcpy(dest, src, n);
}


 
static void vmw_adjust_rect(struct vmw_diff_cpy *diff, size_t diff_offs)
{
	size_t offs = (diff_offs + diff->line_offset) / diff->cpp;
	struct drm_rect *rect = &diff->rect;

	rect->x1 = min_t(int, rect->x1, offs);
	rect->x2 = max_t(int, rect->x2, offs + 1);
	rect->y1 = min_t(int, rect->y1, diff->line);
	rect->y2 = max_t(int, rect->y2, diff->line + 1);
}

 
void vmw_diff_memcpy(struct vmw_diff_cpy *diff, u8 *dest, const u8 *src,
		     size_t n)
{
	ssize_t csize, byte_len;

	if (WARN_ON_ONCE(round_down(n, diff->cpp) != n))
		return;

	 
	csize = vmw_find_first_diff(dest, src, n, diff->cpp);
	if (csize < n) {
		vmw_adjust_rect(diff, csize);
		byte_len = diff->cpp;

		 
		diff->line_offset += csize;
		dest += csize;
		src += csize;
		n -= csize;
		csize = vmw_find_last_diff(dest, src, n, diff->cpp);
		if (csize >= 0) {
			byte_len += csize;
			vmw_adjust_rect(diff, csize);
		}
		memcpy(dest, src, byte_len);
	}
	diff->line_offset += n;
}

 
struct vmw_bo_blit_line_data {
	u32 mapped_dst;
	u8 *dst_addr;
	struct page **dst_pages;
	u32 dst_num_pages;
	pgprot_t dst_prot;
	u32 mapped_src;
	u8 *src_addr;
	struct page **src_pages;
	u32 src_num_pages;
	pgprot_t src_prot;
	struct vmw_diff_cpy *diff;
};

 
static int vmw_bo_cpu_blit_line(struct vmw_bo_blit_line_data *d,
				u32 dst_offset,
				u32 src_offset,
				u32 bytes_to_copy)
{
	struct vmw_diff_cpy *diff = d->diff;

	while (bytes_to_copy) {
		u32 copy_size = bytes_to_copy;
		u32 dst_page = dst_offset >> PAGE_SHIFT;
		u32 src_page = src_offset >> PAGE_SHIFT;
		u32 dst_page_offset = dst_offset & ~PAGE_MASK;
		u32 src_page_offset = src_offset & ~PAGE_MASK;
		bool unmap_dst = d->dst_addr && dst_page != d->mapped_dst;
		bool unmap_src = d->src_addr && (src_page != d->mapped_src ||
						 unmap_dst);

		copy_size = min_t(u32, copy_size, PAGE_SIZE - dst_page_offset);
		copy_size = min_t(u32, copy_size, PAGE_SIZE - src_page_offset);

		if (unmap_src) {
			kunmap_atomic(d->src_addr);
			d->src_addr = NULL;
		}

		if (unmap_dst) {
			kunmap_atomic(d->dst_addr);
			d->dst_addr = NULL;
		}

		if (!d->dst_addr) {
			if (WARN_ON_ONCE(dst_page >= d->dst_num_pages))
				return -EINVAL;

			d->dst_addr =
				kmap_atomic_prot(d->dst_pages[dst_page],
						 d->dst_prot);
			if (!d->dst_addr)
				return -ENOMEM;

			d->mapped_dst = dst_page;
		}

		if (!d->src_addr) {
			if (WARN_ON_ONCE(src_page >= d->src_num_pages))
				return -EINVAL;

			d->src_addr =
				kmap_atomic_prot(d->src_pages[src_page],
						 d->src_prot);
			if (!d->src_addr)
				return -ENOMEM;

			d->mapped_src = src_page;
		}
		diff->do_cpy(diff, d->dst_addr + dst_page_offset,
			     d->src_addr + src_page_offset, copy_size);

		bytes_to_copy -= copy_size;
		dst_offset += copy_size;
		src_offset += copy_size;
	}

	return 0;
}

 
int vmw_bo_cpu_blit(struct ttm_buffer_object *dst,
		    u32 dst_offset, u32 dst_stride,
		    struct ttm_buffer_object *src,
		    u32 src_offset, u32 src_stride,
		    u32 w, u32 h,
		    struct vmw_diff_cpy *diff)
{
	struct ttm_operation_ctx ctx = {
		.interruptible = false,
		.no_wait_gpu = false
	};
	u32 j, initial_line = dst_offset / dst_stride;
	struct vmw_bo_blit_line_data d;
	int ret = 0;

	 
	if (!(dst->pin_count))
		dma_resv_assert_held(dst->base.resv);
	if (!(src->pin_count))
		dma_resv_assert_held(src->base.resv);

	if (!ttm_tt_is_populated(dst->ttm)) {
		ret = dst->bdev->funcs->ttm_tt_populate(dst->bdev, dst->ttm, &ctx);
		if (ret)
			return ret;
	}

	if (!ttm_tt_is_populated(src->ttm)) {
		ret = src->bdev->funcs->ttm_tt_populate(src->bdev, src->ttm, &ctx);
		if (ret)
			return ret;
	}

	d.mapped_dst = 0;
	d.mapped_src = 0;
	d.dst_addr = NULL;
	d.src_addr = NULL;
	d.dst_pages = dst->ttm->pages;
	d.src_pages = src->ttm->pages;
	d.dst_num_pages = PFN_UP(dst->resource->size);
	d.src_num_pages = PFN_UP(src->resource->size);
	d.dst_prot = ttm_io_prot(dst, dst->resource, PAGE_KERNEL);
	d.src_prot = ttm_io_prot(src, src->resource, PAGE_KERNEL);
	d.diff = diff;

	for (j = 0; j < h; ++j) {
		diff->line = j + initial_line;
		diff->line_offset = dst_offset % dst_stride;
		ret = vmw_bo_cpu_blit_line(&d, dst_offset, src_offset, w);
		if (ret)
			goto out;

		dst_offset += dst_stride;
		src_offset += src_stride;
	}
out:
	if (d.src_addr)
		kunmap_atomic(d.src_addr);
	if (d.dst_addr)
		kunmap_atomic(d.dst_addr);

	return ret;
}
