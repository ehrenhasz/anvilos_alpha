#ifndef __AMDGPU_RES_CURSOR_H__
#define __AMDGPU_RES_CURSOR_H__
#include <drm/drm_mm.h>
#include <drm/ttm/ttm_resource.h>
#include <drm/ttm/ttm_range_manager.h>
#include "amdgpu_vram_mgr.h"
struct amdgpu_res_cursor {
	uint64_t		start;
	uint64_t		size;
	uint64_t		remaining;
	void			*node;
	uint32_t		mem_type;
};
static inline void amdgpu_res_first(struct ttm_resource *res,
				    uint64_t start, uint64_t size,
				    struct amdgpu_res_cursor *cur)
{
	struct drm_buddy_block *block;
	struct list_head *head, *next;
	struct drm_mm_node *node;
	if (!res)
		goto fallback;
	BUG_ON(start + size > res->size);
	cur->mem_type = res->mem_type;
	switch (cur->mem_type) {
	case TTM_PL_VRAM:
		head = &to_amdgpu_vram_mgr_resource(res)->blocks;
		block = list_first_entry_or_null(head,
						 struct drm_buddy_block,
						 link);
		if (!block)
			goto fallback;
		while (start >= amdgpu_vram_mgr_block_size(block)) {
			start -= amdgpu_vram_mgr_block_size(block);
			next = block->link.next;
			if (next != head)
				block = list_entry(next, struct drm_buddy_block, link);
		}
		cur->start = amdgpu_vram_mgr_block_start(block) + start;
		cur->size = min(amdgpu_vram_mgr_block_size(block) - start, size);
		cur->remaining = size;
		cur->node = block;
		break;
	case TTM_PL_TT:
	case AMDGPU_PL_DOORBELL:
		node = to_ttm_range_mgr_node(res)->mm_nodes;
		while (start >= node->size << PAGE_SHIFT)
			start -= node++->size << PAGE_SHIFT;
		cur->start = (node->start << PAGE_SHIFT) + start;
		cur->size = min((node->size << PAGE_SHIFT) - start, size);
		cur->remaining = size;
		cur->node = node;
		break;
	default:
		goto fallback;
	}
	return;
fallback:
	cur->start = start;
	cur->size = size;
	cur->remaining = size;
	cur->node = NULL;
	WARN_ON(res && start + size > res->size);
	return;
}
static inline void amdgpu_res_next(struct amdgpu_res_cursor *cur, uint64_t size)
{
	struct drm_buddy_block *block;
	struct drm_mm_node *node;
	struct list_head *next;
	BUG_ON(size > cur->remaining);
	cur->remaining -= size;
	if (!cur->remaining)
		return;
	cur->size -= size;
	if (cur->size) {
		cur->start += size;
		return;
	}
	switch (cur->mem_type) {
	case TTM_PL_VRAM:
		block = cur->node;
		next = block->link.next;
		block = list_entry(next, struct drm_buddy_block, link);
		cur->node = block;
		cur->start = amdgpu_vram_mgr_block_start(block);
		cur->size = min(amdgpu_vram_mgr_block_size(block), cur->remaining);
		break;
	case TTM_PL_TT:
	case AMDGPU_PL_DOORBELL:
		node = cur->node;
		cur->node = ++node;
		cur->start = node->start << PAGE_SHIFT;
		cur->size = min(node->size << PAGE_SHIFT, cur->remaining);
		break;
	default:
		return;
	}
}
#endif
