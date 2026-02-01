
 

#include <linux/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/gfp.h>
#include <linux/kmsan.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/usb.h>

#include "../internal.h"
#include "../slab.h"
#include "kmsan.h"

 

void kmsan_task_create(struct task_struct *task)
{
	kmsan_enter_runtime();
	kmsan_internal_task_create(task);
	kmsan_leave_runtime();
}

void kmsan_task_exit(struct task_struct *task)
{
	struct kmsan_ctx *ctx = &task->kmsan_ctx;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	ctx->allow_reporting = false;
}

void kmsan_slab_alloc(struct kmem_cache *s, void *object, gfp_t flags)
{
	if (unlikely(object == NULL))
		return;
	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	 
	if (s->ctor || (s->flags & SLAB_TYPESAFE_BY_RCU))
		return;

	kmsan_enter_runtime();
	if (flags & __GFP_ZERO)
		kmsan_internal_unpoison_memory(object, s->object_size,
					       KMSAN_POISON_CHECK);
	else
		kmsan_internal_poison_memory(object, s->object_size, flags,
					     KMSAN_POISON_CHECK);
	kmsan_leave_runtime();
}

void kmsan_slab_free(struct kmem_cache *s, void *object)
{
	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	 
	if (unlikely(s->flags & (SLAB_TYPESAFE_BY_RCU | SLAB_POISON)))
		return;
	 
	if (s->ctor)
		return;
	kmsan_enter_runtime();
	kmsan_internal_poison_memory(object, s->object_size, GFP_KERNEL,
				     KMSAN_POISON_CHECK | KMSAN_POISON_FREE);
	kmsan_leave_runtime();
}

void kmsan_kmalloc_large(const void *ptr, size_t size, gfp_t flags)
{
	if (unlikely(ptr == NULL))
		return;
	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	kmsan_enter_runtime();
	if (flags & __GFP_ZERO)
		kmsan_internal_unpoison_memory((void *)ptr, size,
					         true);
	else
		kmsan_internal_poison_memory((void *)ptr, size, flags,
					     KMSAN_POISON_CHECK);
	kmsan_leave_runtime();
}

void kmsan_kfree_large(const void *ptr)
{
	struct page *page;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	kmsan_enter_runtime();
	page = virt_to_head_page((void *)ptr);
	KMSAN_WARN_ON(ptr != page_address(page));
	kmsan_internal_poison_memory((void *)ptr,
				     page_size(page),
				     GFP_KERNEL,
				     KMSAN_POISON_CHECK | KMSAN_POISON_FREE);
	kmsan_leave_runtime();
}

static unsigned long vmalloc_shadow(unsigned long addr)
{
	return (unsigned long)kmsan_get_metadata((void *)addr,
						 KMSAN_META_SHADOW);
}

static unsigned long vmalloc_origin(unsigned long addr)
{
	return (unsigned long)kmsan_get_metadata((void *)addr,
						 KMSAN_META_ORIGIN);
}

void kmsan_vunmap_range_noflush(unsigned long start, unsigned long end)
{
	__vunmap_range_noflush(vmalloc_shadow(start), vmalloc_shadow(end));
	__vunmap_range_noflush(vmalloc_origin(start), vmalloc_origin(end));
	flush_cache_vmap(vmalloc_shadow(start), vmalloc_shadow(end));
	flush_cache_vmap(vmalloc_origin(start), vmalloc_origin(end));
}

 
int kmsan_ioremap_page_range(unsigned long start, unsigned long end,
			     phys_addr_t phys_addr, pgprot_t prot,
			     unsigned int page_shift)
{
	gfp_t gfp_mask = GFP_KERNEL | __GFP_ZERO;
	struct page *shadow, *origin;
	unsigned long off = 0;
	int nr, err = 0, clean = 0, mapped;

	if (!kmsan_enabled || kmsan_in_runtime())
		return 0;

	nr = (end - start) / PAGE_SIZE;
	kmsan_enter_runtime();
	for (int i = 0; i < nr; i++, off += PAGE_SIZE, clean = i) {
		shadow = alloc_pages(gfp_mask, 1);
		origin = alloc_pages(gfp_mask, 1);
		if (!shadow || !origin) {
			err = -ENOMEM;
			goto ret;
		}
		mapped = __vmap_pages_range_noflush(
			vmalloc_shadow(start + off),
			vmalloc_shadow(start + off + PAGE_SIZE), prot, &shadow,
			PAGE_SHIFT);
		if (mapped) {
			err = mapped;
			goto ret;
		}
		shadow = NULL;
		mapped = __vmap_pages_range_noflush(
			vmalloc_origin(start + off),
			vmalloc_origin(start + off + PAGE_SIZE), prot, &origin,
			PAGE_SHIFT);
		if (mapped) {
			__vunmap_range_noflush(
				vmalloc_shadow(start + off),
				vmalloc_shadow(start + off + PAGE_SIZE));
			err = mapped;
			goto ret;
		}
		origin = NULL;
	}
	 
	clean = 0;

ret:
	if (clean > 0) {
		 
		if (shadow)
			__free_pages(shadow, 1);
		if (origin)
			__free_pages(origin, 1);
		__vunmap_range_noflush(
			vmalloc_shadow(start),
			vmalloc_shadow(start + clean * PAGE_SIZE));
		__vunmap_range_noflush(
			vmalloc_origin(start),
			vmalloc_origin(start + clean * PAGE_SIZE));
	}
	flush_cache_vmap(vmalloc_shadow(start), vmalloc_shadow(end));
	flush_cache_vmap(vmalloc_origin(start), vmalloc_origin(end));
	kmsan_leave_runtime();
	return err;
}

void kmsan_iounmap_page_range(unsigned long start, unsigned long end)
{
	unsigned long v_shadow, v_origin;
	struct page *shadow, *origin;
	int nr;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	nr = (end - start) / PAGE_SIZE;
	kmsan_enter_runtime();
	v_shadow = (unsigned long)vmalloc_shadow(start);
	v_origin = (unsigned long)vmalloc_origin(start);
	for (int i = 0; i < nr;
	     i++, v_shadow += PAGE_SIZE, v_origin += PAGE_SIZE) {
		shadow = kmsan_vmalloc_to_page_or_null((void *)v_shadow);
		origin = kmsan_vmalloc_to_page_or_null((void *)v_origin);
		__vunmap_range_noflush(v_shadow, vmalloc_shadow(end));
		__vunmap_range_noflush(v_origin, vmalloc_origin(end));
		if (shadow)
			__free_pages(shadow, 1);
		if (origin)
			__free_pages(origin, 1);
	}
	flush_cache_vmap(vmalloc_shadow(start), vmalloc_shadow(end));
	flush_cache_vmap(vmalloc_origin(start), vmalloc_origin(end));
	kmsan_leave_runtime();
}

void kmsan_copy_to_user(void __user *to, const void *from, size_t to_copy,
			size_t left)
{
	unsigned long ua_flags;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	 

	 
	if (!to_copy)
		return;
	 
	if (to_copy <= left)
		return;

	ua_flags = user_access_save();
	if ((u64)to < TASK_SIZE) {
		 
		kmsan_internal_check_memory((void *)from, to_copy - left, to,
					    REASON_COPY_TO_USER);
	} else {
		 
		kmsan_internal_memmove_metadata((void *)to, (void *)from,
						to_copy - left);
	}
	user_access_restore(ua_flags);
}
EXPORT_SYMBOL(kmsan_copy_to_user);

 
void kmsan_handle_urb(const struct urb *urb, bool is_out)
{
	if (!urb)
		return;
	if (is_out)
		kmsan_internal_check_memory(urb->transfer_buffer,
					    urb->transfer_buffer_length,
					      0, REASON_SUBMIT_URB);
	else
		kmsan_internal_unpoison_memory(urb->transfer_buffer,
					       urb->transfer_buffer_length,
					         false);
}
EXPORT_SYMBOL_GPL(kmsan_handle_urb);

static void kmsan_handle_dma_page(const void *addr, size_t size,
				  enum dma_data_direction dir)
{
	switch (dir) {
	case DMA_BIDIRECTIONAL:
		kmsan_internal_check_memory((void *)addr, size,   0,
					    REASON_ANY);
		kmsan_internal_unpoison_memory((void *)addr, size,
					         false);
		break;
	case DMA_TO_DEVICE:
		kmsan_internal_check_memory((void *)addr, size,   0,
					    REASON_ANY);
		break;
	case DMA_FROM_DEVICE:
		kmsan_internal_unpoison_memory((void *)addr, size,
					         false);
		break;
	case DMA_NONE:
		break;
	}
}

 
void kmsan_handle_dma(struct page *page, size_t offset, size_t size,
		      enum dma_data_direction dir)
{
	u64 page_offset, to_go, addr;

	if (PageHighMem(page))
		return;
	addr = (u64)page_address(page) + offset;
	 
	while (size > 0) {
		page_offset = offset_in_page(addr);
		to_go = min(PAGE_SIZE - page_offset, (u64)size);
		kmsan_handle_dma_page((void *)addr, to_go, dir);
		addr += to_go;
		size -= to_go;
	}
}

void kmsan_handle_dma_sg(struct scatterlist *sg, int nents,
			 enum dma_data_direction dir)
{
	struct scatterlist *item;
	int i;

	for_each_sg(sg, item, nents, i)
		kmsan_handle_dma(sg_page(item), item->offset, item->length,
				 dir);
}

 
void kmsan_poison_memory(const void *address, size_t size, gfp_t flags)
{
	if (!kmsan_enabled || kmsan_in_runtime())
		return;
	kmsan_enter_runtime();
	 
	kmsan_internal_poison_memory((void *)address, size, flags,
				     KMSAN_POISON_NOCHECK);
	kmsan_leave_runtime();
}
EXPORT_SYMBOL(kmsan_poison_memory);

void kmsan_unpoison_memory(const void *address, size_t size)
{
	unsigned long ua_flags;

	if (!kmsan_enabled || kmsan_in_runtime())
		return;

	ua_flags = user_access_save();
	kmsan_enter_runtime();
	 
	kmsan_internal_unpoison_memory((void *)address, size,
				       KMSAN_POISON_NOCHECK);
	kmsan_leave_runtime();
	user_access_restore(ua_flags);
}
EXPORT_SYMBOL(kmsan_unpoison_memory);

 
void kmsan_unpoison_entry_regs(const struct pt_regs *regs)
{
	unsigned long ua_flags;

	if (!kmsan_enabled)
		return;

	ua_flags = user_access_save();
	kmsan_internal_unpoison_memory((void *)regs, sizeof(*regs),
				       KMSAN_POISON_NOCHECK);
	user_access_restore(ua_flags);
}

void kmsan_check_memory(const void *addr, size_t size)
{
	if (!kmsan_enabled)
		return;
	return kmsan_internal_check_memory((void *)addr, size,   0,
					   REASON_ANY);
}
EXPORT_SYMBOL(kmsan_check_memory);
