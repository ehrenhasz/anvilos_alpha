
 

#include <drm/drm_drv.h>

#include "amdgpu.h"
#include "amdgpu_trace.h"
#include "amdgpu_vm.h"

 
struct amdgpu_vm_pt_cursor {
	uint64_t pfn;
	struct amdgpu_vm_bo_base *parent;
	struct amdgpu_vm_bo_base *entry;
	unsigned int level;
};

 
static unsigned int amdgpu_vm_pt_level_shift(struct amdgpu_device *adev,
					     unsigned int level)
{
	switch (level) {
	case AMDGPU_VM_PDB2:
	case AMDGPU_VM_PDB1:
	case AMDGPU_VM_PDB0:
		return 9 * (AMDGPU_VM_PDB0 - level) +
			adev->vm_manager.block_size;
	case AMDGPU_VM_PTB:
		return 0;
	default:
		return ~0;
	}
}

 
static unsigned int amdgpu_vm_pt_num_entries(struct amdgpu_device *adev,
					     unsigned int level)
{
	unsigned int shift;

	shift = amdgpu_vm_pt_level_shift(adev, adev->vm_manager.root_level);
	if (level == adev->vm_manager.root_level)
		 
		return round_up(adev->vm_manager.max_pfn, 1ULL << shift)
			>> shift;
	else if (level != AMDGPU_VM_PTB)
		 
		return 512;

	 
	return AMDGPU_VM_PTE_COUNT(adev);
}

 
static unsigned int amdgpu_vm_pt_num_ats_entries(struct amdgpu_device *adev)
{
	unsigned int shift;

	shift = amdgpu_vm_pt_level_shift(adev, adev->vm_manager.root_level);
	return AMDGPU_GMC_HOLE_START >> (shift + AMDGPU_GPU_PAGE_SHIFT);
}

 
static uint32_t amdgpu_vm_pt_entries_mask(struct amdgpu_device *adev,
					  unsigned int level)
{
	if (level <= adev->vm_manager.root_level)
		return 0xffffffff;
	else if (level != AMDGPU_VM_PTB)
		return 0x1ff;
	else
		return AMDGPU_VM_PTE_COUNT(adev) - 1;
}

 
static unsigned int amdgpu_vm_pt_size(struct amdgpu_device *adev,
				      unsigned int level)
{
	return AMDGPU_GPU_PAGE_ALIGN(amdgpu_vm_pt_num_entries(adev, level) * 8);
}

 
static struct amdgpu_vm_bo_base *
amdgpu_vm_pt_parent(struct amdgpu_vm_bo_base *pt)
{
	struct amdgpu_bo *parent = pt->bo->parent;

	if (!parent)
		return NULL;

	return parent->vm_bo;
}

 
static void amdgpu_vm_pt_start(struct amdgpu_device *adev,
			       struct amdgpu_vm *vm, uint64_t start,
			       struct amdgpu_vm_pt_cursor *cursor)
{
	cursor->pfn = start;
	cursor->parent = NULL;
	cursor->entry = &vm->root;
	cursor->level = adev->vm_manager.root_level;
}

 
static bool amdgpu_vm_pt_descendant(struct amdgpu_device *adev,
				    struct amdgpu_vm_pt_cursor *cursor)
{
	unsigned int mask, shift, idx;

	if ((cursor->level == AMDGPU_VM_PTB) || !cursor->entry ||
	    !cursor->entry->bo)
		return false;

	mask = amdgpu_vm_pt_entries_mask(adev, cursor->level);
	shift = amdgpu_vm_pt_level_shift(adev, cursor->level);

	++cursor->level;
	idx = (cursor->pfn >> shift) & mask;
	cursor->parent = cursor->entry;
	cursor->entry = &to_amdgpu_bo_vm(cursor->entry->bo)->entries[idx];
	return true;
}

 
static bool amdgpu_vm_pt_sibling(struct amdgpu_device *adev,
				 struct amdgpu_vm_pt_cursor *cursor)
{

	unsigned int shift, num_entries;
	struct amdgpu_bo_vm *parent;

	 
	if (!cursor->parent)
		return false;

	 
	shift = amdgpu_vm_pt_level_shift(adev, cursor->level - 1);
	num_entries = amdgpu_vm_pt_num_entries(adev, cursor->level - 1);
	parent = to_amdgpu_bo_vm(cursor->parent->bo);

	if (cursor->entry == &parent->entries[num_entries - 1])
		return false;

	cursor->pfn += 1ULL << shift;
	cursor->pfn &= ~((1ULL << shift) - 1);
	++cursor->entry;
	return true;
}

 
static bool amdgpu_vm_pt_ancestor(struct amdgpu_vm_pt_cursor *cursor)
{
	if (!cursor->parent)
		return false;

	--cursor->level;
	cursor->entry = cursor->parent;
	cursor->parent = amdgpu_vm_pt_parent(cursor->parent);
	return true;
}

 
static void amdgpu_vm_pt_next(struct amdgpu_device *adev,
			      struct amdgpu_vm_pt_cursor *cursor)
{
	 
	if (amdgpu_vm_pt_descendant(adev, cursor))
		return;

	 
	while (!amdgpu_vm_pt_sibling(adev, cursor)) {
		 
		if (!amdgpu_vm_pt_ancestor(cursor)) {
			cursor->pfn = ~0ll;
			return;
		}
	}
}

 
static void amdgpu_vm_pt_first_dfs(struct amdgpu_device *adev,
				   struct amdgpu_vm *vm,
				   struct amdgpu_vm_pt_cursor *start,
				   struct amdgpu_vm_pt_cursor *cursor)
{
	if (start)
		*cursor = *start;
	else
		amdgpu_vm_pt_start(adev, vm, 0, cursor);

	while (amdgpu_vm_pt_descendant(adev, cursor))
		;
}

 
static bool amdgpu_vm_pt_continue_dfs(struct amdgpu_vm_pt_cursor *start,
				      struct amdgpu_vm_bo_base *entry)
{
	return entry && (!start || entry != start->entry);
}

 
static void amdgpu_vm_pt_next_dfs(struct amdgpu_device *adev,
				  struct amdgpu_vm_pt_cursor *cursor)
{
	if (!cursor->entry)
		return;

	if (!cursor->parent)
		cursor->entry = NULL;
	else if (amdgpu_vm_pt_sibling(adev, cursor))
		while (amdgpu_vm_pt_descendant(adev, cursor))
			;
	else
		amdgpu_vm_pt_ancestor(cursor);
}

 
#define for_each_amdgpu_vm_pt_dfs_safe(adev, vm, start, cursor, entry)		\
	for (amdgpu_vm_pt_first_dfs((adev), (vm), (start), &(cursor)),		\
	     (entry) = (cursor).entry, amdgpu_vm_pt_next_dfs((adev), &(cursor));\
	     amdgpu_vm_pt_continue_dfs((start), (entry));			\
	     (entry) = (cursor).entry, amdgpu_vm_pt_next_dfs((adev), &(cursor)))

 
int amdgpu_vm_pt_clear(struct amdgpu_device *adev, struct amdgpu_vm *vm,
		       struct amdgpu_bo_vm *vmbo, bool immediate)
{
	unsigned int level = adev->vm_manager.root_level;
	struct ttm_operation_ctx ctx = { true, false };
	struct amdgpu_vm_update_params params;
	struct amdgpu_bo *ancestor = &vmbo->bo;
	unsigned int entries, ats_entries;
	struct amdgpu_bo *bo = &vmbo->bo;
	uint64_t addr;
	int r, idx;

	 
	if (ancestor->parent) {
		++level;
		while (ancestor->parent->parent) {
			++level;
			ancestor = ancestor->parent;
		}
	}

	entries = amdgpu_bo_size(bo) / 8;
	if (!vm->pte_support_ats) {
		ats_entries = 0;

	} else if (!bo->parent) {
		ats_entries = amdgpu_vm_pt_num_ats_entries(adev);
		ats_entries = min(ats_entries, entries);
		entries -= ats_entries;

	} else {
		struct amdgpu_vm_bo_base *pt;

		pt = ancestor->vm_bo;
		ats_entries = amdgpu_vm_pt_num_ats_entries(adev);
		if ((pt - to_amdgpu_bo_vm(vm->root.bo)->entries) >=
		    ats_entries) {
			ats_entries = 0;
		} else {
			ats_entries = entries;
			entries = 0;
		}
	}

	r = ttm_bo_validate(&bo->tbo, &bo->placement, &ctx);
	if (r)
		return r;

	if (vmbo->shadow) {
		struct amdgpu_bo *shadow = vmbo->shadow;

		r = ttm_bo_validate(&shadow->tbo, &shadow->placement, &ctx);
		if (r)
			return r;
	}

	if (!drm_dev_enter(adev_to_drm(adev), &idx))
		return -ENODEV;

	r = vm->update_funcs->map_table(vmbo);
	if (r)
		goto exit;

	memset(&params, 0, sizeof(params));
	params.adev = adev;
	params.vm = vm;
	params.immediate = immediate;

	r = vm->update_funcs->prepare(&params, NULL, AMDGPU_SYNC_EXPLICIT);
	if (r)
		goto exit;

	addr = 0;
	if (ats_entries) {
		uint64_t value = 0, flags;

		flags = AMDGPU_PTE_DEFAULT_ATC;
		if (level != AMDGPU_VM_PTB) {
			 
			flags |= AMDGPU_PDE_PTE;
			amdgpu_gmc_get_vm_pde(adev, level, &value, &flags);
		}

		r = vm->update_funcs->update(&params, vmbo, addr, 0,
					     ats_entries, value, flags);
		if (r)
			goto exit;

		addr += ats_entries * 8;
	}

	if (entries) {
		uint64_t value = 0, flags = 0;

		if (adev->asic_type >= CHIP_VEGA10) {
			if (level != AMDGPU_VM_PTB) {
				 
				flags |= AMDGPU_PDE_PTE;
				amdgpu_gmc_get_vm_pde(adev, level,
						      &value, &flags);
			} else {
				 
				flags = AMDGPU_PTE_EXECUTABLE;
			}
		}

		r = vm->update_funcs->update(&params, vmbo, addr, 0, entries,
					     value, flags);
		if (r)
			goto exit;
	}

	r = vm->update_funcs->commit(&params, NULL);
exit:
	drm_dev_exit(idx);
	return r;
}

 
int amdgpu_vm_pt_create(struct amdgpu_device *adev, struct amdgpu_vm *vm,
			int level, bool immediate, struct amdgpu_bo_vm **vmbo,
			int32_t xcp_id)
{
	struct amdgpu_bo_param bp;
	struct amdgpu_bo *bo;
	struct dma_resv *resv;
	unsigned int num_entries;
	int r;

	memset(&bp, 0, sizeof(bp));

	bp.size = amdgpu_vm_pt_size(adev, level);
	bp.byte_align = AMDGPU_GPU_PAGE_SIZE;

	if (!adev->gmc.is_app_apu)
		bp.domain = AMDGPU_GEM_DOMAIN_VRAM;
	else
		bp.domain = AMDGPU_GEM_DOMAIN_GTT;

	bp.domain = amdgpu_bo_get_preferred_domain(adev, bp.domain);
	bp.flags = AMDGPU_GEM_CREATE_VRAM_CONTIGUOUS |
		AMDGPU_GEM_CREATE_CPU_GTT_USWC;

	if (level < AMDGPU_VM_PTB)
		num_entries = amdgpu_vm_pt_num_entries(adev, level);
	else
		num_entries = 0;

	bp.bo_ptr_size = struct_size((*vmbo), entries, num_entries);

	if (vm->use_cpu_for_update)
		bp.flags |= AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED;

	bp.type = ttm_bo_type_kernel;
	bp.no_wait_gpu = immediate;
	bp.xcp_id_plus1 = xcp_id + 1;

	if (vm->root.bo)
		bp.resv = vm->root.bo->tbo.base.resv;

	r = amdgpu_bo_create_vm(adev, &bp, vmbo);
	if (r)
		return r;

	bo = &(*vmbo)->bo;
	if (vm->is_compute_context || (adev->flags & AMD_IS_APU)) {
		(*vmbo)->shadow = NULL;
		return 0;
	}

	if (!bp.resv)
		WARN_ON(dma_resv_lock(bo->tbo.base.resv,
				      NULL));
	resv = bp.resv;
	memset(&bp, 0, sizeof(bp));
	bp.size = amdgpu_vm_pt_size(adev, level);
	bp.domain = AMDGPU_GEM_DOMAIN_GTT;
	bp.flags = AMDGPU_GEM_CREATE_CPU_GTT_USWC;
	bp.type = ttm_bo_type_kernel;
	bp.resv = bo->tbo.base.resv;
	bp.bo_ptr_size = sizeof(struct amdgpu_bo);
	bp.xcp_id_plus1 = xcp_id + 1;

	r = amdgpu_bo_create(adev, &bp, &(*vmbo)->shadow);

	if (!resv)
		dma_resv_unlock(bo->tbo.base.resv);

	if (r) {
		amdgpu_bo_unref(&bo);
		return r;
	}

	amdgpu_bo_add_to_shadow_list(*vmbo);

	return 0;
}

 
static int amdgpu_vm_pt_alloc(struct amdgpu_device *adev,
			      struct amdgpu_vm *vm,
			      struct amdgpu_vm_pt_cursor *cursor,
			      bool immediate)
{
	struct amdgpu_vm_bo_base *entry = cursor->entry;
	struct amdgpu_bo *pt_bo;
	struct amdgpu_bo_vm *pt;
	int r;

	if (entry->bo)
		return 0;

	amdgpu_vm_eviction_unlock(vm);
	r = amdgpu_vm_pt_create(adev, vm, cursor->level, immediate, &pt,
				vm->root.bo->xcp_id);
	amdgpu_vm_eviction_lock(vm);
	if (r)
		return r;

	 
	pt_bo = &pt->bo;
	pt_bo->parent = amdgpu_bo_ref(cursor->parent->bo);
	amdgpu_vm_bo_base_init(entry, vm, pt_bo);
	r = amdgpu_vm_pt_clear(adev, vm, pt, immediate);
	if (r)
		goto error_free_pt;

	return 0;

error_free_pt:
	amdgpu_bo_unref(&pt->shadow);
	amdgpu_bo_unref(&pt_bo);
	return r;
}

 
static void amdgpu_vm_pt_free(struct amdgpu_vm_bo_base *entry)
{
	struct amdgpu_bo *shadow;

	if (!entry->bo)
		return;

	entry->bo->vm_bo = NULL;
	shadow = amdgpu_bo_shadowed(entry->bo);
	if (shadow) {
		ttm_bo_set_bulk_move(&shadow->tbo, NULL);
		amdgpu_bo_unref(&shadow);
	}
	ttm_bo_set_bulk_move(&entry->bo->tbo, NULL);

	spin_lock(&entry->vm->status_lock);
	list_del(&entry->vm_status);
	spin_unlock(&entry->vm->status_lock);
	amdgpu_bo_unref(&entry->bo);
}

void amdgpu_vm_pt_free_work(struct work_struct *work)
{
	struct amdgpu_vm_bo_base *entry, *next;
	struct amdgpu_vm *vm;
	LIST_HEAD(pt_freed);

	vm = container_of(work, struct amdgpu_vm, pt_free_work);

	spin_lock(&vm->status_lock);
	list_splice_init(&vm->pt_freed, &pt_freed);
	spin_unlock(&vm->status_lock);

	 
	amdgpu_bo_reserve(vm->root.bo, true);

	list_for_each_entry_safe(entry, next, &pt_freed, vm_status)
		amdgpu_vm_pt_free(entry);

	amdgpu_bo_unreserve(vm->root.bo);
}

 
static void amdgpu_vm_pt_free_dfs(struct amdgpu_device *adev,
				  struct amdgpu_vm *vm,
				  struct amdgpu_vm_pt_cursor *start,
				  bool unlocked)
{
	struct amdgpu_vm_pt_cursor cursor;
	struct amdgpu_vm_bo_base *entry;

	if (unlocked) {
		spin_lock(&vm->status_lock);
		for_each_amdgpu_vm_pt_dfs_safe(adev, vm, start, cursor, entry)
			list_move(&entry->vm_status, &vm->pt_freed);

		if (start)
			list_move(&start->entry->vm_status, &vm->pt_freed);
		spin_unlock(&vm->status_lock);
		schedule_work(&vm->pt_free_work);
		return;
	}

	for_each_amdgpu_vm_pt_dfs_safe(adev, vm, start, cursor, entry)
		amdgpu_vm_pt_free(entry);

	if (start)
		amdgpu_vm_pt_free(start->entry);
}

 
void amdgpu_vm_pt_free_root(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	amdgpu_vm_pt_free_dfs(adev, vm, NULL, false);
}

 
bool amdgpu_vm_pt_is_root_clean(struct amdgpu_device *adev,
				struct amdgpu_vm *vm)
{
	enum amdgpu_vm_level root = adev->vm_manager.root_level;
	unsigned int entries = amdgpu_vm_pt_num_entries(adev, root);
	unsigned int i = 0;

	for (i = 0; i < entries; i++) {
		if (to_amdgpu_bo_vm(vm->root.bo)->entries[i].bo)
			return false;
	}
	return true;
}

 
int amdgpu_vm_pde_update(struct amdgpu_vm_update_params *params,
			 struct amdgpu_vm_bo_base *entry)
{
	struct amdgpu_vm_bo_base *parent = amdgpu_vm_pt_parent(entry);
	struct amdgpu_bo *bo = parent->bo, *pbo;
	struct amdgpu_vm *vm = params->vm;
	uint64_t pde, pt, flags;
	unsigned int level;

	for (level = 0, pbo = bo->parent; pbo; ++level)
		pbo = pbo->parent;

	level += params->adev->vm_manager.root_level;
	amdgpu_gmc_get_pde_for_bo(entry->bo, level, &pt, &flags);
	pde = (entry - to_amdgpu_bo_vm(parent->bo)->entries) * 8;
	return vm->update_funcs->update(params, to_amdgpu_bo_vm(bo), pde, pt,
					1, 0, flags);
}

 
static void amdgpu_vm_pte_update_noretry_flags(struct amdgpu_device *adev,
						uint64_t *flags)
{
	 
	if ((*flags & AMDGPU_VM_NORETRY_FLAGS) == AMDGPU_VM_NORETRY_FLAGS) {
		*flags &= ~AMDGPU_VM_NORETRY_FLAGS;
		*flags |= adev->gmc.noretry_flags;
	}
}

 
static void amdgpu_vm_pte_update_flags(struct amdgpu_vm_update_params *params,
				       struct amdgpu_bo_vm *pt,
				       unsigned int level,
				       uint64_t pe, uint64_t addr,
				       unsigned int count, uint32_t incr,
				       uint64_t flags)
{
	struct amdgpu_device *adev = params->adev;

	if (level != AMDGPU_VM_PTB) {
		flags |= AMDGPU_PDE_PTE;
		amdgpu_gmc_get_vm_pde(adev, level, &addr, &flags);

	} else if (adev->asic_type >= CHIP_VEGA10 &&
		   !(flags & AMDGPU_PTE_VALID) &&
		   !(flags & AMDGPU_PTE_PRT)) {

		 
		flags |= AMDGPU_PTE_EXECUTABLE;
	}

	 
	if (level == AMDGPU_VM_PTB)
		amdgpu_vm_pte_update_noretry_flags(adev, &flags);

	 
	if ((flags & AMDGPU_PTE_SYSTEM) && (adev->flags & AMD_IS_APU) &&
	    adev->gmc.gmc_funcs->override_vm_pte_flags &&
	    num_possible_nodes() > 1) {
		if (!params->pages_addr)
			amdgpu_gmc_override_vm_pte_flags(adev, params->vm,
							 addr, &flags);
		else
			dev_dbg(adev->dev,
				"override_vm_pte_flags skipped: non-contiguous\n");
	}

	params->vm->update_funcs->update(params, pt, pe, addr, count, incr,
					 flags);
}

 
static void amdgpu_vm_pte_fragment(struct amdgpu_vm_update_params *params,
				   uint64_t start, uint64_t end, uint64_t flags,
				   unsigned int *frag, uint64_t *frag_end)
{
	 
	unsigned int max_frag;

	if (params->adev->asic_type < CHIP_VEGA10)
		max_frag = params->adev->vm_manager.fragment_size;
	else
		max_frag = 31;

	 
	if (params->pages_addr) {
		*frag = 0;
		*frag_end = end;
		return;
	}

	 
	*frag = min_t(unsigned int, ffs(start) - 1, fls64(end - start) - 1);
	if (*frag >= max_frag) {
		*frag = max_frag;
		*frag_end = end & ~((1ULL << max_frag) - 1);
	} else {
		*frag_end = start + (1 << *frag);
	}
}

 
int amdgpu_vm_ptes_update(struct amdgpu_vm_update_params *params,
			  uint64_t start, uint64_t end,
			  uint64_t dst, uint64_t flags)
{
	struct amdgpu_device *adev = params->adev;
	struct amdgpu_vm_pt_cursor cursor;
	uint64_t frag_start = start, frag_end;
	unsigned int frag;
	int r;

	 
	amdgpu_vm_pte_fragment(params, frag_start, end, flags, &frag,
			       &frag_end);

	 
	amdgpu_vm_pt_start(adev, params->vm, start, &cursor);
	while (cursor.pfn < end) {
		unsigned int shift, parent_shift, mask;
		uint64_t incr, entry_end, pe_start;
		struct amdgpu_bo *pt;

		if (!params->unlocked) {
			 
			r = amdgpu_vm_pt_alloc(params->adev, params->vm,
					       &cursor, params->immediate);
			if (r)
				return r;
		}

		shift = amdgpu_vm_pt_level_shift(adev, cursor.level);
		parent_shift = amdgpu_vm_pt_level_shift(adev, cursor.level - 1);
		if (params->unlocked) {
			 
			if (amdgpu_vm_pt_descendant(adev, &cursor))
				continue;
		} else if (adev->asic_type < CHIP_VEGA10 &&
			   (flags & AMDGPU_PTE_VALID)) {
			 
			if (cursor.level != AMDGPU_VM_PTB) {
				if (!amdgpu_vm_pt_descendant(adev, &cursor))
					return -ENOENT;
				continue;
			}
		} else if (frag < shift) {
			 
			if (amdgpu_vm_pt_descendant(adev, &cursor))
				continue;
		} else if (frag >= parent_shift) {
			 
			if (!amdgpu_vm_pt_ancestor(&cursor))
				return -EINVAL;
			continue;
		}

		pt = cursor.entry->bo;
		if (!pt) {
			 
			if (flags & AMDGPU_PTE_VALID)
				return -ENOENT;

			 
			if (!amdgpu_vm_pt_ancestor(&cursor))
				return -EINVAL;

			pt = cursor.entry->bo;
			shift = parent_shift;
			frag_end = max(frag_end, ALIGN(frag_start + 1,
				   1ULL << shift));
		}

		 
		incr = (uint64_t)AMDGPU_GPU_PAGE_SIZE << shift;
		mask = amdgpu_vm_pt_entries_mask(adev, cursor.level);
		pe_start = ((cursor.pfn >> shift) & mask) * 8;
		entry_end = ((uint64_t)mask + 1) << shift;
		entry_end += cursor.pfn & ~(entry_end - 1);
		entry_end = min(entry_end, end);

		do {
			struct amdgpu_vm *vm = params->vm;
			uint64_t upd_end = min(entry_end, frag_end);
			unsigned int nptes = (upd_end - frag_start) >> shift;
			uint64_t upd_flags = flags | AMDGPU_PTE_FRAG(frag);

			 
			nptes = max(nptes, 1u);

			trace_amdgpu_vm_update_ptes(params, frag_start, upd_end,
						    min(nptes, 32u), dst, incr,
						    upd_flags,
						    vm->task_info.tgid,
						    vm->immediate.fence_context);
			amdgpu_vm_pte_update_flags(params, to_amdgpu_bo_vm(pt),
						   cursor.level, pe_start, dst,
						   nptes, incr, upd_flags);

			pe_start += nptes * 8;
			dst += nptes * incr;

			frag_start = upd_end;
			if (frag_start >= frag_end) {
				 
				amdgpu_vm_pte_fragment(params, frag_start, end,
						       flags, &frag, &frag_end);
				if (frag < shift)
					break;
			}
		} while (frag_start < entry_end);

		if (amdgpu_vm_pt_descendant(adev, &cursor)) {
			 
			while (cursor.pfn < frag_start) {
				 
				if (cursor.entry->bo) {
					params->table_freed = true;
					amdgpu_vm_pt_free_dfs(adev, params->vm,
							      &cursor,
							      params->unlocked);
				}
				amdgpu_vm_pt_next(adev, &cursor);
			}

		} else if (frag >= shift) {
			 
			amdgpu_vm_pt_next(adev, &cursor);
		}
	}

	return 0;
}

 
int amdgpu_vm_pt_map_tables(struct amdgpu_device *adev, struct amdgpu_vm *vm)
{
	struct amdgpu_vm_pt_cursor cursor;
	struct amdgpu_vm_bo_base *entry;

	for_each_amdgpu_vm_pt_dfs_safe(adev, vm, NULL, cursor, entry) {

		struct amdgpu_bo_vm *bo;
		int r;

		if (entry->bo) {
			bo = to_amdgpu_bo_vm(entry->bo);
			r = vm->update_funcs->map_table(bo);
			if (r)
				return r;
		}
	}

	return 0;
}
