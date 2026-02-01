
 

#include "vmwgfx_bo.h"
#include "vmwgfx_drv.h"

#include <linux/highmem.h>

#ifdef CONFIG_64BIT
#define VMW_PPN_SIZE 8
#define VMW_MOBFMT_PTDEPTH_0 SVGA3D_MOBFMT_PT64_0
#define VMW_MOBFMT_PTDEPTH_1 SVGA3D_MOBFMT_PT64_1
#define VMW_MOBFMT_PTDEPTH_2 SVGA3D_MOBFMT_PT64_2
#else
#define VMW_PPN_SIZE 4
#define VMW_MOBFMT_PTDEPTH_0 SVGA3D_MOBFMT_PT_0
#define VMW_MOBFMT_PTDEPTH_1 SVGA3D_MOBFMT_PT_1
#define VMW_MOBFMT_PTDEPTH_2 SVGA3D_MOBFMT_PT_2
#endif

 
struct vmw_mob {
	struct vmw_bo *pt_bo;
	unsigned long num_pages;
	unsigned pt_level;
	dma_addr_t pt_root_page;
	uint32_t id;
};

 
static const struct vmw_otable pre_dx_tables[] = {
	{VMWGFX_NUM_MOB * sizeof(SVGAOTableMobEntry), NULL, true},
	{VMWGFX_NUM_GB_SURFACE * sizeof(SVGAOTableSurfaceEntry), NULL, true},
	{VMWGFX_NUM_GB_CONTEXT * sizeof(SVGAOTableContextEntry), NULL, true},
	{VMWGFX_NUM_GB_SHADER * sizeof(SVGAOTableShaderEntry), NULL, true},
	{VMWGFX_NUM_GB_SCREEN_TARGET * sizeof(SVGAOTableScreenTargetEntry),
	 NULL, true}
};

static const struct vmw_otable dx_tables[] = {
	{VMWGFX_NUM_MOB * sizeof(SVGAOTableMobEntry), NULL, true},
	{VMWGFX_NUM_GB_SURFACE * sizeof(SVGAOTableSurfaceEntry), NULL, true},
	{VMWGFX_NUM_GB_CONTEXT * sizeof(SVGAOTableContextEntry), NULL, true},
	{VMWGFX_NUM_GB_SHADER * sizeof(SVGAOTableShaderEntry), NULL, true},
	{VMWGFX_NUM_GB_SCREEN_TARGET * sizeof(SVGAOTableScreenTargetEntry),
	 NULL, true},
	{VMWGFX_NUM_DXCONTEXT * sizeof(SVGAOTableDXContextEntry), NULL, true},
};

static int vmw_mob_pt_populate(struct vmw_private *dev_priv,
			       struct vmw_mob *mob);
static void vmw_mob_pt_setup(struct vmw_mob *mob,
			     struct vmw_piter data_iter,
			     unsigned long num_data_pages);


static inline void vmw_bo_unpin_unlocked(struct ttm_buffer_object *bo)
{
	int ret = ttm_bo_reserve(bo, false, true, NULL);
	BUG_ON(ret != 0);
	ttm_bo_unpin(bo);
	ttm_bo_unreserve(bo);
}


 
static int vmw_setup_otable_base(struct vmw_private *dev_priv,
				 SVGAOTableType type,
				 struct ttm_buffer_object *otable_bo,
				 unsigned long offset,
				 struct vmw_otable *otable)
{
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdSetOTableBase64 body;
	} *cmd;
	struct vmw_mob *mob;
	const struct vmw_sg_table *vsgt;
	struct vmw_piter iter;
	int ret;

	BUG_ON(otable->page_table != NULL);

	vsgt = vmw_bo_sg_table(otable_bo);
	vmw_piter_start(&iter, vsgt, offset >> PAGE_SHIFT);
	WARN_ON(!vmw_piter_next(&iter));

	mob = vmw_mob_create(otable->size >> PAGE_SHIFT);
	if (unlikely(mob == NULL)) {
		DRM_ERROR("Failed creating OTable page table.\n");
		return -ENOMEM;
	}

	if (otable->size <= PAGE_SIZE) {
		mob->pt_level = VMW_MOBFMT_PTDEPTH_0;
		mob->pt_root_page = vmw_piter_dma_addr(&iter);
	} else {
		ret = vmw_mob_pt_populate(dev_priv, mob);
		if (unlikely(ret != 0))
			goto out_no_populate;

		vmw_mob_pt_setup(mob, iter, otable->size >> PAGE_SHIFT);
		mob->pt_level += VMW_MOBFMT_PTDEPTH_1 - SVGA3D_MOBFMT_PT_1;
	}

	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (unlikely(cmd == NULL)) {
		ret = -ENOMEM;
		goto out_no_fifo;
	}

	memset(cmd, 0, sizeof(*cmd));
	cmd->header.id = SVGA_3D_CMD_SET_OTABLE_BASE64;
	cmd->header.size = sizeof(cmd->body);
	cmd->body.type = type;
	cmd->body.baseAddress = mob->pt_root_page >> PAGE_SHIFT;
	cmd->body.sizeInBytes = otable->size;
	cmd->body.validSizeInBytes = 0;
	cmd->body.ptDepth = mob->pt_level;

	 
	BUG_ON(mob->pt_level == VMW_MOBFMT_PTDEPTH_2);

	vmw_cmd_commit(dev_priv, sizeof(*cmd));
	otable->page_table = mob;

	return 0;

out_no_fifo:
out_no_populate:
	vmw_mob_destroy(mob);
	return ret;
}

 
static void vmw_takedown_otable_base(struct vmw_private *dev_priv,
				     SVGAOTableType type,
				     struct vmw_otable *otable)
{
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdSetOTableBase body;
	} *cmd;
	struct ttm_buffer_object *bo;

	if (otable->page_table == NULL)
		return;

	bo = &otable->page_table->pt_bo->tbo;
	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (unlikely(cmd == NULL))
		return;

	memset(cmd, 0, sizeof(*cmd));
	cmd->header.id = SVGA_3D_CMD_SET_OTABLE_BASE;
	cmd->header.size = sizeof(cmd->body);
	cmd->body.type = type;
	cmd->body.baseAddress = 0;
	cmd->body.sizeInBytes = 0;
	cmd->body.validSizeInBytes = 0;
	cmd->body.ptDepth = SVGA3D_MOBFMT_INVALID;
	vmw_cmd_commit(dev_priv, sizeof(*cmd));

	if (bo) {
		int ret;

		ret = ttm_bo_reserve(bo, false, true, NULL);
		BUG_ON(ret != 0);

		vmw_bo_fence_single(bo, NULL);
		ttm_bo_unreserve(bo);
	}

	vmw_mob_destroy(otable->page_table);
	otable->page_table = NULL;
}


static int vmw_otable_batch_setup(struct vmw_private *dev_priv,
				  struct vmw_otable_batch *batch)
{
	unsigned long offset;
	unsigned long bo_size;
	struct vmw_otable *otables = batch->otables;
	SVGAOTableType i;
	int ret;

	bo_size = 0;
	for (i = 0; i < batch->num_otables; ++i) {
		if (!otables[i].enabled)
			continue;

		otables[i].size = PFN_ALIGN(otables[i].size);
		bo_size += otables[i].size;
	}

	ret = vmw_bo_create_and_populate(dev_priv, bo_size,
					 VMW_BO_DOMAIN_WAITABLE_SYS,
					 &batch->otable_bo);
	if (unlikely(ret != 0))
		return ret;

	offset = 0;
	for (i = 0; i < batch->num_otables; ++i) {
		if (!batch->otables[i].enabled)
			continue;

		ret = vmw_setup_otable_base(dev_priv, i,
					    &batch->otable_bo->tbo,
					    offset,
					    &otables[i]);
		if (unlikely(ret != 0))
			goto out_no_setup;
		offset += otables[i].size;
	}

	return 0;

out_no_setup:
	for (i = 0; i < batch->num_otables; ++i) {
		if (batch->otables[i].enabled)
			vmw_takedown_otable_base(dev_priv, i,
						 &batch->otables[i]);
	}

	vmw_bo_unpin_unlocked(&batch->otable_bo->tbo);
	ttm_bo_put(&batch->otable_bo->tbo);
	batch->otable_bo = NULL;
	return ret;
}

 
int vmw_otables_setup(struct vmw_private *dev_priv)
{
	struct vmw_otable **otables = &dev_priv->otable_batch.otables;
	int ret;

	if (has_sm4_context(dev_priv)) {
		*otables = kmemdup(dx_tables, sizeof(dx_tables), GFP_KERNEL);
		if (!(*otables))
			return -ENOMEM;

		dev_priv->otable_batch.num_otables = ARRAY_SIZE(dx_tables);
	} else {
		*otables = kmemdup(pre_dx_tables, sizeof(pre_dx_tables),
				   GFP_KERNEL);
		if (!(*otables))
			return -ENOMEM;

		dev_priv->otable_batch.num_otables = ARRAY_SIZE(pre_dx_tables);
	}

	ret = vmw_otable_batch_setup(dev_priv, &dev_priv->otable_batch);
	if (unlikely(ret != 0))
		goto out_setup;

	return 0;

out_setup:
	kfree(*otables);
	return ret;
}

static void vmw_otable_batch_takedown(struct vmw_private *dev_priv,
			       struct vmw_otable_batch *batch)
{
	SVGAOTableType i;
	struct ttm_buffer_object *bo = &batch->otable_bo->tbo;
	int ret;

	for (i = 0; i < batch->num_otables; ++i)
		if (batch->otables[i].enabled)
			vmw_takedown_otable_base(dev_priv, i,
						 &batch->otables[i]);

	ret = ttm_bo_reserve(bo, false, true, NULL);
	BUG_ON(ret != 0);

	vmw_bo_fence_single(bo, NULL);
	ttm_bo_unpin(bo);
	ttm_bo_unreserve(bo);

	vmw_bo_unreference(&batch->otable_bo);
}

 
void vmw_otables_takedown(struct vmw_private *dev_priv)
{
	vmw_otable_batch_takedown(dev_priv, &dev_priv->otable_batch);
	kfree(dev_priv->otable_batch.otables);
}

 
static unsigned long vmw_mob_calculate_pt_pages(unsigned long data_pages)
{
	unsigned long data_size = data_pages * PAGE_SIZE;
	unsigned long tot_size = 0;

	while (likely(data_size > PAGE_SIZE)) {
		data_size = DIV_ROUND_UP(data_size, PAGE_SIZE);
		data_size *= VMW_PPN_SIZE;
		tot_size += PFN_ALIGN(data_size);
	}

	return tot_size >> PAGE_SHIFT;
}

 
struct vmw_mob *vmw_mob_create(unsigned long data_pages)
{
	struct vmw_mob *mob = kzalloc(sizeof(*mob), GFP_KERNEL);

	if (unlikely(!mob))
		return NULL;

	mob->num_pages = vmw_mob_calculate_pt_pages(data_pages);

	return mob;
}

 
static int vmw_mob_pt_populate(struct vmw_private *dev_priv,
			       struct vmw_mob *mob)
{
	BUG_ON(mob->pt_bo != NULL);

	return vmw_bo_create_and_populate(dev_priv, mob->num_pages * PAGE_SIZE,
					  VMW_BO_DOMAIN_WAITABLE_SYS,
					  &mob->pt_bo);
}

 
#if (VMW_PPN_SIZE == 8)
static void vmw_mob_assign_ppn(u32 **addr, dma_addr_t val)
{
	*((u64 *) *addr) = val >> PAGE_SHIFT;
	*addr += 2;
}
#else
static void vmw_mob_assign_ppn(u32 **addr, dma_addr_t val)
{
	*(*addr)++ = val >> PAGE_SHIFT;
}
#endif

 
static unsigned long vmw_mob_build_pt(struct vmw_piter *data_iter,
				      unsigned long num_data_pages,
				      struct vmw_piter *pt_iter)
{
	unsigned long pt_size = num_data_pages * VMW_PPN_SIZE;
	unsigned long num_pt_pages = DIV_ROUND_UP(pt_size, PAGE_SIZE);
	unsigned long pt_page;
	u32 *addr, *save_addr;
	unsigned long i;
	struct page *page;

	for (pt_page = 0; pt_page < num_pt_pages; ++pt_page) {
		page = vmw_piter_page(pt_iter);

		save_addr = addr = kmap_atomic(page);

		for (i = 0; i < PAGE_SIZE / VMW_PPN_SIZE; ++i) {
			vmw_mob_assign_ppn(&addr,
					   vmw_piter_dma_addr(data_iter));
			if (unlikely(--num_data_pages == 0))
				break;
			WARN_ON(!vmw_piter_next(data_iter));
		}
		kunmap_atomic(save_addr);
		vmw_piter_next(pt_iter);
	}

	return num_pt_pages;
}

 
static void vmw_mob_pt_setup(struct vmw_mob *mob,
			     struct vmw_piter data_iter,
			     unsigned long num_data_pages)
{
	unsigned long num_pt_pages = 0;
	struct ttm_buffer_object *bo = &mob->pt_bo->tbo;
	struct vmw_piter save_pt_iter = {0};
	struct vmw_piter pt_iter;
	const struct vmw_sg_table *vsgt;
	int ret;

	BUG_ON(num_data_pages == 0);

	ret = ttm_bo_reserve(bo, false, true, NULL);
	BUG_ON(ret != 0);

	vsgt = vmw_bo_sg_table(bo);
	vmw_piter_start(&pt_iter, vsgt, 0);
	BUG_ON(!vmw_piter_next(&pt_iter));
	mob->pt_level = 0;
	while (likely(num_data_pages > 1)) {
		++mob->pt_level;
		BUG_ON(mob->pt_level > 2);
		save_pt_iter = pt_iter;
		num_pt_pages = vmw_mob_build_pt(&data_iter, num_data_pages,
						&pt_iter);
		data_iter = save_pt_iter;
		num_data_pages = num_pt_pages;
	}

	mob->pt_root_page = vmw_piter_dma_addr(&save_pt_iter);
	ttm_bo_unreserve(bo);
}

 
void vmw_mob_destroy(struct vmw_mob *mob)
{
	if (mob->pt_bo) {
		vmw_bo_unpin_unlocked(&mob->pt_bo->tbo);
		vmw_bo_unreference(&mob->pt_bo);
	}
	kfree(mob);
}

 
void vmw_mob_unbind(struct vmw_private *dev_priv,
		    struct vmw_mob *mob)
{
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDestroyGBMob body;
	} *cmd;
	int ret;
	struct ttm_buffer_object *bo = &mob->pt_bo->tbo;

	if (bo) {
		ret = ttm_bo_reserve(bo, false, true, NULL);
		 
		BUG_ON(ret != 0);
	}

	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (cmd) {
		cmd->header.id = SVGA_3D_CMD_DESTROY_GB_MOB;
		cmd->header.size = sizeof(cmd->body);
		cmd->body.mobid = mob->id;
		vmw_cmd_commit(dev_priv, sizeof(*cmd));
	}

	if (bo) {
		vmw_bo_fence_single(bo, NULL);
		ttm_bo_unreserve(bo);
	}
	vmw_fifo_resource_dec(dev_priv);
}

 
int vmw_mob_bind(struct vmw_private *dev_priv,
		 struct vmw_mob *mob,
		 const struct vmw_sg_table *vsgt,
		 unsigned long num_data_pages,
		 int32_t mob_id)
{
	int ret;
	bool pt_set_up = false;
	struct vmw_piter data_iter;
	struct {
		SVGA3dCmdHeader header;
		SVGA3dCmdDefineGBMob64 body;
	} *cmd;

	mob->id = mob_id;
	vmw_piter_start(&data_iter, vsgt, 0);
	if (unlikely(!vmw_piter_next(&data_iter)))
		return 0;

	if (likely(num_data_pages == 1)) {
		mob->pt_level = VMW_MOBFMT_PTDEPTH_0;
		mob->pt_root_page = vmw_piter_dma_addr(&data_iter);
	} else if (unlikely(mob->pt_bo == NULL)) {
		ret = vmw_mob_pt_populate(dev_priv, mob);
		if (unlikely(ret != 0))
			return ret;

		vmw_mob_pt_setup(mob, data_iter, num_data_pages);
		pt_set_up = true;
		mob->pt_level += VMW_MOBFMT_PTDEPTH_1 - SVGA3D_MOBFMT_PT_1;
	}

	vmw_fifo_resource_inc(dev_priv);

	cmd = VMW_CMD_RESERVE(dev_priv, sizeof(*cmd));
	if (unlikely(cmd == NULL))
		goto out_no_cmd_space;

	cmd->header.id = SVGA_3D_CMD_DEFINE_GB_MOB64;
	cmd->header.size = sizeof(cmd->body);
	cmd->body.mobid = mob_id;
	cmd->body.ptDepth = mob->pt_level;
	cmd->body.base = mob->pt_root_page >> PAGE_SHIFT;
	cmd->body.sizeInBytes = num_data_pages * PAGE_SIZE;

	vmw_cmd_commit(dev_priv, sizeof(*cmd));

	return 0;

out_no_cmd_space:
	vmw_fifo_resource_dec(dev_priv);
	if (pt_set_up) {
		vmw_bo_unpin_unlocked(&mob->pt_bo->tbo);
		vmw_bo_unreference(&mob->pt_bo);
	}

	return -ENOMEM;
}
