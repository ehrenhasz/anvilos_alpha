
 

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/gfp.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include "vsp1.h"
#include "vsp1_dl.h"

#define VSP1_DL_NUM_ENTRIES		256

#define VSP1_DLH_INT_ENABLE		(1 << 1)
#define VSP1_DLH_AUTO_START		(1 << 0)

#define VSP1_DLH_EXT_PRE_CMD_EXEC	(1 << 9)
#define VSP1_DLH_EXT_POST_CMD_EXEC	(1 << 8)

struct vsp1_dl_header_list {
	u32 num_bytes;
	u32 addr;
} __packed;

struct vsp1_dl_header {
	u32 num_lists;
	struct vsp1_dl_header_list lists[8];
	u32 next_header;
	u32 flags;
} __packed;

 
struct vsp1_dl_ext_header {
	u32 padding;

	 
	u16 pre_ext_dl_num_cmd;
	u16 flags;
	u32 pre_ext_dl_plist;

	u32 post_ext_dl_num_cmd;
	u32 post_ext_dl_plist;
} __packed;

struct vsp1_dl_header_extended {
	struct vsp1_dl_header header;
	struct vsp1_dl_ext_header ext;
} __packed;

struct vsp1_dl_entry {
	u32 addr;
	u32 data;
} __packed;

 
struct vsp1_pre_ext_dl_body {
	u32 opcode;
	u32 flags;
	u32 address_set;
	u32 reserved;
} __packed;

 
struct vsp1_dl_body {
	struct list_head list;
	struct list_head free;

	refcount_t refcnt;

	struct vsp1_dl_body_pool *pool;

	struct vsp1_dl_entry *entries;
	dma_addr_t dma;
	size_t size;

	unsigned int num_entries;
	unsigned int max_entries;
};

 
struct vsp1_dl_body_pool {
	 
	dma_addr_t dma;
	size_t size;
	void *mem;

	 
	struct vsp1_dl_body *bodies;
	struct list_head free;
	spinlock_t lock;

	struct vsp1_device *vsp1;
};

 
struct vsp1_dl_cmd_pool {
	 
	dma_addr_t dma;
	size_t size;
	void *mem;

	struct vsp1_dl_ext_cmd *cmds;
	struct list_head free;

	spinlock_t lock;

	struct vsp1_device *vsp1;
};

 
struct vsp1_dl_list {
	struct list_head list;
	struct vsp1_dl_manager *dlm;

	struct vsp1_dl_header *header;
	struct vsp1_dl_ext_header *extension;
	dma_addr_t dma;

	struct vsp1_dl_body *body0;
	struct list_head bodies;

	struct vsp1_dl_ext_cmd *pre_cmd;
	struct vsp1_dl_ext_cmd *post_cmd;

	bool has_chain;
	struct list_head chain;

	unsigned int flags;
};

 
struct vsp1_dl_manager {
	unsigned int index;
	bool singleshot;
	struct vsp1_device *vsp1;

	spinlock_t lock;
	struct list_head free;
	struct vsp1_dl_list *active;
	struct vsp1_dl_list *queued;
	struct vsp1_dl_list *pending;

	struct vsp1_dl_body_pool *pool;
	struct vsp1_dl_cmd_pool *cmdpool;
};

 

 
struct vsp1_dl_body_pool *
vsp1_dl_body_pool_create(struct vsp1_device *vsp1, unsigned int num_bodies,
			 unsigned int num_entries, size_t extra_size)
{
	struct vsp1_dl_body_pool *pool;
	size_t dlb_size;
	unsigned int i;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->vsp1 = vsp1;

	 
	dlb_size = num_entries * sizeof(struct vsp1_dl_entry) + extra_size;
	pool->size = dlb_size * num_bodies;

	pool->bodies = kcalloc(num_bodies, sizeof(*pool->bodies), GFP_KERNEL);
	if (!pool->bodies) {
		kfree(pool);
		return NULL;
	}

	pool->mem = dma_alloc_wc(vsp1->bus_master, pool->size, &pool->dma,
				 GFP_KERNEL);
	if (!pool->mem) {
		kfree(pool->bodies);
		kfree(pool);
		return NULL;
	}

	spin_lock_init(&pool->lock);
	INIT_LIST_HEAD(&pool->free);

	for (i = 0; i < num_bodies; ++i) {
		struct vsp1_dl_body *dlb = &pool->bodies[i];

		dlb->pool = pool;
		dlb->max_entries = num_entries;

		dlb->dma = pool->dma + i * dlb_size;
		dlb->entries = pool->mem + i * dlb_size;

		list_add_tail(&dlb->free, &pool->free);
	}

	return pool;
}

 
void vsp1_dl_body_pool_destroy(struct vsp1_dl_body_pool *pool)
{
	if (!pool)
		return;

	if (pool->mem)
		dma_free_wc(pool->vsp1->bus_master, pool->size, pool->mem,
			    pool->dma);

	kfree(pool->bodies);
	kfree(pool);
}

 
struct vsp1_dl_body *vsp1_dl_body_get(struct vsp1_dl_body_pool *pool)
{
	struct vsp1_dl_body *dlb = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);

	if (!list_empty(&pool->free)) {
		dlb = list_first_entry(&pool->free, struct vsp1_dl_body, free);
		list_del(&dlb->free);
		refcount_set(&dlb->refcnt, 1);
	}

	spin_unlock_irqrestore(&pool->lock, flags);

	return dlb;
}

 
void vsp1_dl_body_put(struct vsp1_dl_body *dlb)
{
	unsigned long flags;

	if (!dlb)
		return;

	if (!refcount_dec_and_test(&dlb->refcnt))
		return;

	dlb->num_entries = 0;

	spin_lock_irqsave(&dlb->pool->lock, flags);
	list_add_tail(&dlb->free, &dlb->pool->free);
	spin_unlock_irqrestore(&dlb->pool->lock, flags);
}

 
void vsp1_dl_body_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	if (WARN_ONCE(dlb->num_entries >= dlb->max_entries,
		      "DLB size exceeded (max %u)", dlb->max_entries))
		return;

	dlb->entries[dlb->num_entries].addr = reg;
	dlb->entries[dlb->num_entries].data = data;
	dlb->num_entries++;
}

 

enum vsp1_extcmd_type {
	VSP1_EXTCMD_AUTODISP,
	VSP1_EXTCMD_AUTOFLD,
};

struct vsp1_extended_command_info {
	u16 opcode;
	size_t body_size;
};

static const struct vsp1_extended_command_info vsp1_extended_commands[] = {
	[VSP1_EXTCMD_AUTODISP] = { 0x02, 96 },
	[VSP1_EXTCMD_AUTOFLD]  = { 0x03, 160 },
};

 
static struct vsp1_dl_cmd_pool *
vsp1_dl_cmd_pool_create(struct vsp1_device *vsp1, enum vsp1_extcmd_type type,
			unsigned int num_cmds)
{
	struct vsp1_dl_cmd_pool *pool;
	unsigned int i;
	size_t cmd_size;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->vsp1 = vsp1;

	spin_lock_init(&pool->lock);
	INIT_LIST_HEAD(&pool->free);

	pool->cmds = kcalloc(num_cmds, sizeof(*pool->cmds), GFP_KERNEL);
	if (!pool->cmds) {
		kfree(pool);
		return NULL;
	}

	cmd_size = sizeof(struct vsp1_pre_ext_dl_body) +
		   vsp1_extended_commands[type].body_size;
	cmd_size = ALIGN(cmd_size, 16);

	pool->size = cmd_size * num_cmds;
	pool->mem = dma_alloc_wc(vsp1->bus_master, pool->size, &pool->dma,
				 GFP_KERNEL);
	if (!pool->mem) {
		kfree(pool->cmds);
		kfree(pool);
		return NULL;
	}

	for (i = 0; i < num_cmds; ++i) {
		struct vsp1_dl_ext_cmd *cmd = &pool->cmds[i];
		size_t cmd_offset = i * cmd_size;
		 
		size_t data_offset = sizeof(struct vsp1_pre_ext_dl_body) +
				     cmd_offset;

		cmd->pool = pool;
		cmd->opcode = vsp1_extended_commands[type].opcode;

		 
		cmd->num_cmds = 1;
		cmd->cmds = pool->mem + cmd_offset;
		cmd->cmd_dma = pool->dma + cmd_offset;

		cmd->data = pool->mem + data_offset;
		cmd->data_dma = pool->dma + data_offset;

		list_add_tail(&cmd->free, &pool->free);
	}

	return pool;
}

static
struct vsp1_dl_ext_cmd *vsp1_dl_ext_cmd_get(struct vsp1_dl_cmd_pool *pool)
{
	struct vsp1_dl_ext_cmd *cmd = NULL;
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);

	if (!list_empty(&pool->free)) {
		cmd = list_first_entry(&pool->free, struct vsp1_dl_ext_cmd,
				       free);
		list_del(&cmd->free);
	}

	spin_unlock_irqrestore(&pool->lock, flags);

	return cmd;
}

static void vsp1_dl_ext_cmd_put(struct vsp1_dl_ext_cmd *cmd)
{
	unsigned long flags;

	if (!cmd)
		return;

	 
	cmd->flags = 0;

	spin_lock_irqsave(&cmd->pool->lock, flags);
	list_add_tail(&cmd->free, &cmd->pool->free);
	spin_unlock_irqrestore(&cmd->pool->lock, flags);
}

static void vsp1_dl_ext_cmd_pool_destroy(struct vsp1_dl_cmd_pool *pool)
{
	if (!pool)
		return;

	if (pool->mem)
		dma_free_wc(pool->vsp1->bus_master, pool->size, pool->mem,
			    pool->dma);

	kfree(pool->cmds);
	kfree(pool);
}

struct vsp1_dl_ext_cmd *vsp1_dl_get_pre_cmd(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	if (dl->pre_cmd)
		return dl->pre_cmd;

	dl->pre_cmd = vsp1_dl_ext_cmd_get(dlm->cmdpool);

	return dl->pre_cmd;
}

 

static struct vsp1_dl_list *vsp1_dl_list_alloc(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl;
	size_t header_offset;

	dl = kzalloc(sizeof(*dl), GFP_KERNEL);
	if (!dl)
		return NULL;

	INIT_LIST_HEAD(&dl->bodies);
	dl->dlm = dlm;

	 
	dl->body0 = vsp1_dl_body_get(dlm->pool);
	if (!dl->body0) {
		kfree(dl);
		return NULL;
	}

	header_offset = dl->body0->max_entries * sizeof(*dl->body0->entries);

	dl->header = ((void *)dl->body0->entries) + header_offset;
	dl->dma = dl->body0->dma + header_offset;

	memset(dl->header, 0, sizeof(*dl->header));
	dl->header->lists[0].addr = dl->body0->dma;

	return dl;
}

static void vsp1_dl_list_bodies_put(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_body *dlb, *tmp;

	list_for_each_entry_safe(dlb, tmp, &dl->bodies, list) {
		list_del(&dlb->list);
		vsp1_dl_body_put(dlb);
	}
}

static void vsp1_dl_list_free(struct vsp1_dl_list *dl)
{
	vsp1_dl_body_put(dl->body0);
	vsp1_dl_list_bodies_put(dl);

	kfree(dl);
}

 
struct vsp1_dl_list *vsp1_dl_list_get(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl = NULL;
	unsigned long flags;

	spin_lock_irqsave(&dlm->lock, flags);

	if (!list_empty(&dlm->free)) {
		dl = list_first_entry(&dlm->free, struct vsp1_dl_list, list);
		list_del(&dl->list);

		 
		INIT_LIST_HEAD(&dl->chain);
	}

	spin_unlock_irqrestore(&dlm->lock, flags);

	return dl;
}

 
static void __vsp1_dl_list_put(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_list *dl_next;

	if (!dl)
		return;

	 
	if (dl->has_chain) {
		list_for_each_entry(dl_next, &dl->chain, chain)
			__vsp1_dl_list_put(dl_next);
	}

	dl->has_chain = false;

	vsp1_dl_list_bodies_put(dl);

	vsp1_dl_ext_cmd_put(dl->pre_cmd);
	vsp1_dl_ext_cmd_put(dl->post_cmd);

	dl->pre_cmd = NULL;
	dl->post_cmd = NULL;

	 
	dl->body0->num_entries = 0;

	list_add_tail(&dl->list, &dl->dlm->free);
}

 
void vsp1_dl_list_put(struct vsp1_dl_list *dl)
{
	unsigned long flags;

	if (!dl)
		return;

	spin_lock_irqsave(&dl->dlm->lock, flags);
	__vsp1_dl_list_put(dl);
	spin_unlock_irqrestore(&dl->dlm->lock, flags);
}

 
struct vsp1_dl_body *vsp1_dl_list_get_body0(struct vsp1_dl_list *dl)
{
	return dl->body0;
}

 
int vsp1_dl_list_add_body(struct vsp1_dl_list *dl, struct vsp1_dl_body *dlb)
{
	refcount_inc(&dlb->refcnt);

	list_add_tail(&dlb->list, &dl->bodies);

	return 0;
}

 
int vsp1_dl_list_add_chain(struct vsp1_dl_list *head,
			   struct vsp1_dl_list *dl)
{
	head->has_chain = true;
	list_add_tail(&dl->chain, &head->chain);
	return 0;
}

static void vsp1_dl_ext_cmd_fill_header(struct vsp1_dl_ext_cmd *cmd)
{
	cmd->cmds[0].opcode = cmd->opcode;
	cmd->cmds[0].flags = cmd->flags;
	cmd->cmds[0].address_set = cmd->data_dma;
	cmd->cmds[0].reserved = 0;
}

static void vsp1_dl_list_fill_header(struct vsp1_dl_list *dl, bool is_last)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_dl_header_list *hdr = dl->header->lists;
	struct vsp1_dl_body *dlb;
	unsigned int num_lists = 0;

	 

	hdr->num_bytes = dl->body0->num_entries
		       * sizeof(*dl->header->lists);

	list_for_each_entry(dlb, &dl->bodies, list) {
		num_lists++;
		hdr++;

		hdr->addr = dlb->dma;
		hdr->num_bytes = dlb->num_entries
			       * sizeof(*dl->header->lists);
	}

	dl->header->num_lists = num_lists;
	dl->header->flags = 0;

	 
	if (!dlm->singleshot || is_last)
		dl->header->flags |= VSP1_DLH_INT_ENABLE;

	 
	if (!dlm->singleshot || !is_last)
		dl->header->flags |= VSP1_DLH_AUTO_START;

	if (!is_last) {
		 
		struct vsp1_dl_list *next = list_next_entry(dl, chain);

		dl->header->next_header = next->dma;
	} else if (!dlm->singleshot) {
		 
		dl->header->next_header = dl->dma;
	}

	if (!dl->extension)
		return;

	dl->extension->flags = 0;

	if (dl->pre_cmd) {
		dl->extension->pre_ext_dl_plist = dl->pre_cmd->cmd_dma;
		dl->extension->pre_ext_dl_num_cmd = dl->pre_cmd->num_cmds;
		dl->extension->flags |= VSP1_DLH_EXT_PRE_CMD_EXEC;

		vsp1_dl_ext_cmd_fill_header(dl->pre_cmd);
	}

	if (dl->post_cmd) {
		dl->extension->post_ext_dl_plist = dl->post_cmd->cmd_dma;
		dl->extension->post_ext_dl_num_cmd = dl->post_cmd->num_cmds;
		dl->extension->flags |= VSP1_DLH_EXT_POST_CMD_EXEC;

		vsp1_dl_ext_cmd_fill_header(dl->post_cmd);
	}
}

static bool vsp1_dl_list_hw_update_pending(struct vsp1_dl_manager *dlm)
{
	struct vsp1_device *vsp1 = dlm->vsp1;

	if (!dlm->queued)
		return false;

	 
	return !!(vsp1_read(vsp1, VI6_CMD(dlm->index)) & VI6_CMD_UPDHDR);
}

static void vsp1_dl_list_hw_enqueue(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_device *vsp1 = dlm->vsp1;

	 
	vsp1_write(vsp1, VI6_DL_HDR_ADDR(dlm->index), dl->dma);
}

static void vsp1_dl_list_commit_continuous(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	 
	if (vsp1_dl_list_hw_update_pending(dlm)) {
		WARN_ON(dlm->pending &&
			(dlm->pending->flags & VSP1_DL_FRAME_END_INTERNAL));
		__vsp1_dl_list_put(dlm->pending);
		dlm->pending = dl;
		return;
	}

	 
	vsp1_dl_list_hw_enqueue(dl);

	__vsp1_dl_list_put(dlm->queued);
	dlm->queued = dl;
}

static void vsp1_dl_list_commit_singleshot(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;

	 
	vsp1_dl_list_hw_enqueue(dl);

	dlm->active = dl;
}

void vsp1_dl_list_commit(struct vsp1_dl_list *dl, unsigned int dl_flags)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_dl_list *dl_next;
	unsigned long flags;

	 
	vsp1_dl_list_fill_header(dl, list_empty(&dl->chain));

	list_for_each_entry(dl_next, &dl->chain, chain) {
		bool last = list_is_last(&dl_next->chain, &dl->chain);

		vsp1_dl_list_fill_header(dl_next, last);
	}

	dl->flags = dl_flags & ~VSP1_DL_FRAME_END_COMPLETED;

	spin_lock_irqsave(&dlm->lock, flags);

	if (dlm->singleshot)
		vsp1_dl_list_commit_singleshot(dl);
	else
		vsp1_dl_list_commit_continuous(dl);

	spin_unlock_irqrestore(&dlm->lock, flags);
}

 

 
unsigned int vsp1_dlm_irq_frame_end(struct vsp1_dl_manager *dlm)
{
	struct vsp1_device *vsp1 = dlm->vsp1;
	u32 status = vsp1_read(vsp1, VI6_STATUS);
	unsigned int flags = 0;

	spin_lock(&dlm->lock);

	 
	if (dlm->singleshot) {
		__vsp1_dl_list_put(dlm->active);
		dlm->active = NULL;
		flags |= VSP1_DL_FRAME_END_COMPLETED;
		goto done;
	}

	 
	if (vsp1_dl_list_hw_update_pending(dlm))
		goto done;

	 
	if (status & VI6_STATUS_FLD_STD(dlm->index))
		goto done;

	 
	if (dlm->active && (dlm->active->flags & VSP1_DL_FRAME_END_WRITEBACK)) {
		flags |= VSP1_DL_FRAME_END_WRITEBACK;
		dlm->active->flags &= ~VSP1_DL_FRAME_END_WRITEBACK;
	}

	 
	if (dlm->queued) {
		if (dlm->queued->flags & VSP1_DL_FRAME_END_INTERNAL)
			flags |= VSP1_DL_FRAME_END_INTERNAL;
		dlm->queued->flags &= ~VSP1_DL_FRAME_END_INTERNAL;

		__vsp1_dl_list_put(dlm->active);
		dlm->active = dlm->queued;
		dlm->queued = NULL;
		flags |= VSP1_DL_FRAME_END_COMPLETED;
	}

	 
	if (dlm->pending) {
		vsp1_dl_list_hw_enqueue(dlm->pending);
		dlm->queued = dlm->pending;
		dlm->pending = NULL;
	}

done:
	spin_unlock(&dlm->lock);

	return flags;
}

 
void vsp1_dlm_setup(struct vsp1_device *vsp1)
{
	unsigned int i;
	u32 ctrl = (256 << VI6_DL_CTRL_AR_WAIT_SHIFT)
		 | VI6_DL_CTRL_DC2 | VI6_DL_CTRL_DC1 | VI6_DL_CTRL_DC0
		 | VI6_DL_CTRL_DLE;
	u32 ext_dl = (0x02 << VI6_DL_EXT_CTRL_POLINT_SHIFT)
		   | VI6_DL_EXT_CTRL_DLPRI | VI6_DL_EXT_CTRL_EXT;

	if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL)) {
		for (i = 0; i < vsp1->info->wpf_count; ++i)
			vsp1_write(vsp1, VI6_DL_EXT_CTRL(i), ext_dl);
	}

	vsp1_write(vsp1, VI6_DL_CTRL, ctrl);
	vsp1_write(vsp1, VI6_DL_SWAP, VI6_DL_SWAP_LWS);
}

void vsp1_dlm_reset(struct vsp1_dl_manager *dlm)
{
	unsigned long flags;

	spin_lock_irqsave(&dlm->lock, flags);

	__vsp1_dl_list_put(dlm->active);
	__vsp1_dl_list_put(dlm->queued);
	__vsp1_dl_list_put(dlm->pending);

	spin_unlock_irqrestore(&dlm->lock, flags);

	dlm->active = NULL;
	dlm->queued = NULL;
	dlm->pending = NULL;
}

struct vsp1_dl_body *vsp1_dlm_dl_body_get(struct vsp1_dl_manager *dlm)
{
	return vsp1_dl_body_get(dlm->pool);
}

struct vsp1_dl_manager *vsp1_dlm_create(struct vsp1_device *vsp1,
					unsigned int index,
					unsigned int prealloc)
{
	struct vsp1_dl_manager *dlm;
	size_t header_size;
	unsigned int i;

	dlm = devm_kzalloc(vsp1->dev, sizeof(*dlm), GFP_KERNEL);
	if (!dlm)
		return NULL;

	dlm->index = index;
	dlm->singleshot = vsp1->info->uapi;
	dlm->vsp1 = vsp1;

	spin_lock_init(&dlm->lock);
	INIT_LIST_HEAD(&dlm->free);

	 
	header_size = vsp1_feature(vsp1, VSP1_HAS_EXT_DL) ?
			sizeof(struct vsp1_dl_header_extended) :
			sizeof(struct vsp1_dl_header);

	header_size = ALIGN(header_size, 8);

	dlm->pool = vsp1_dl_body_pool_create(vsp1, prealloc + 1,
					     VSP1_DL_NUM_ENTRIES, header_size);
	if (!dlm->pool)
		return NULL;

	for (i = 0; i < prealloc; ++i) {
		struct vsp1_dl_list *dl;

		dl = vsp1_dl_list_alloc(dlm);
		if (!dl) {
			vsp1_dlm_destroy(dlm);
			return NULL;
		}

		 
		if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL))
			dl->extension = (void *)dl->header
				      + sizeof(*dl->header);

		list_add_tail(&dl->list, &dlm->free);
	}

	if (vsp1_feature(vsp1, VSP1_HAS_EXT_DL)) {
		dlm->cmdpool = vsp1_dl_cmd_pool_create(vsp1,
					VSP1_EXTCMD_AUTOFLD, prealloc);
		if (!dlm->cmdpool) {
			vsp1_dlm_destroy(dlm);
			return NULL;
		}
	}

	return dlm;
}

void vsp1_dlm_destroy(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl, *next;

	if (!dlm)
		return;

	list_for_each_entry_safe(dl, next, &dlm->free, list) {
		list_del(&dl->list);
		vsp1_dl_list_free(dl);
	}

	vsp1_dl_body_pool_destroy(dlm->pool);
	vsp1_dl_ext_cmd_pool_destroy(dlm->cmdpool);
}
