

#include <linux/irq.h>
#include <linux/module.h>
#include <linux/ntb.h>
#include <linux/msi.h>
#include <linux/pci.h>

struct ntb_msi {
	u64 base_addr;
	u64 end_addr;

	void (*desc_changed)(void *ctx);

	u32 __iomem *peer_mws[];
};

 
int ntb_msi_init(struct ntb_dev *ntb,
		 void (*desc_changed)(void *ctx))
{
	phys_addr_t mw_phys_addr;
	resource_size_t mw_size;
	int peer_widx;
	int peers;
	int ret;
	int i;

	peers = ntb_peer_port_count(ntb);
	if (peers <= 0)
		return -EINVAL;

	ntb->msi = devm_kzalloc(&ntb->dev, struct_size(ntb->msi, peer_mws, peers),
				GFP_KERNEL);
	if (!ntb->msi)
		return -ENOMEM;

	ntb->msi->desc_changed = desc_changed;

	for (i = 0; i < peers; i++) {
		peer_widx = ntb_peer_mw_count(ntb) - 1 - i;

		ret = ntb_peer_mw_get_addr(ntb, peer_widx, &mw_phys_addr,
					   &mw_size);
		if (ret)
			goto unroll;

		ntb->msi->peer_mws[i] = devm_ioremap(&ntb->dev, mw_phys_addr,
						     mw_size);
		if (!ntb->msi->peer_mws[i]) {
			ret = -EFAULT;
			goto unroll;
		}
	}

	return 0;

unroll:
	for (i = 0; i < peers; i++)
		if (ntb->msi->peer_mws[i])
			devm_iounmap(&ntb->dev, ntb->msi->peer_mws[i]);

	devm_kfree(&ntb->dev, ntb->msi);
	ntb->msi = NULL;
	return ret;
}
EXPORT_SYMBOL(ntb_msi_init);

 
int ntb_msi_setup_mws(struct ntb_dev *ntb)
{
	struct msi_desc *desc;
	u64 addr;
	int peer, peer_widx;
	resource_size_t addr_align, size_align, size_max;
	resource_size_t mw_size = SZ_32K;
	resource_size_t mw_min_size = mw_size;
	int i;
	int ret;

	if (!ntb->msi)
		return -EINVAL;

	msi_lock_descs(&ntb->pdev->dev);
	desc = msi_first_desc(&ntb->pdev->dev, MSI_DESC_ASSOCIATED);
	addr = desc->msg.address_lo + ((uint64_t)desc->msg.address_hi << 32);
	msi_unlock_descs(&ntb->pdev->dev);

	for (peer = 0; peer < ntb_peer_port_count(ntb); peer++) {
		peer_widx = ntb_peer_highest_mw_idx(ntb, peer);
		if (peer_widx < 0)
			return peer_widx;

		ret = ntb_mw_get_align(ntb, peer, peer_widx, &addr_align,
				       NULL, NULL);
		if (ret)
			return ret;

		addr &= ~(addr_align - 1);
	}

	for (peer = 0; peer < ntb_peer_port_count(ntb); peer++) {
		peer_widx = ntb_peer_highest_mw_idx(ntb, peer);
		if (peer_widx < 0) {
			ret = peer_widx;
			goto error_out;
		}

		ret = ntb_mw_get_align(ntb, peer, peer_widx, NULL,
				       &size_align, &size_max);
		if (ret)
			goto error_out;

		mw_size = round_up(mw_size, size_align);
		mw_size = max(mw_size, size_max);
		if (mw_size < mw_min_size)
			mw_min_size = mw_size;

		ret = ntb_mw_set_trans(ntb, peer, peer_widx,
				       addr, mw_size);
		if (ret)
			goto error_out;
	}

	ntb->msi->base_addr = addr;
	ntb->msi->end_addr = addr + mw_min_size;

	return 0;

error_out:
	for (i = 0; i < peer; i++) {
		peer_widx = ntb_peer_highest_mw_idx(ntb, peer);
		if (peer_widx < 0)
			continue;

		ntb_mw_clear_trans(ntb, i, peer_widx);
	}

	return ret;
}
EXPORT_SYMBOL(ntb_msi_setup_mws);

 
void ntb_msi_clear_mws(struct ntb_dev *ntb)
{
	int peer;
	int peer_widx;

	for (peer = 0; peer < ntb_peer_port_count(ntb); peer++) {
		peer_widx = ntb_peer_highest_mw_idx(ntb, peer);
		if (peer_widx < 0)
			continue;

		ntb_mw_clear_trans(ntb, peer, peer_widx);
	}
}
EXPORT_SYMBOL(ntb_msi_clear_mws);

struct ntb_msi_devres {
	struct ntb_dev *ntb;
	struct msi_desc *entry;
	struct ntb_msi_desc *msi_desc;
};

static int ntb_msi_set_desc(struct ntb_dev *ntb, struct msi_desc *entry,
			    struct ntb_msi_desc *msi_desc)
{
	u64 addr;

	addr = entry->msg.address_lo +
		((uint64_t)entry->msg.address_hi << 32);

	if (addr < ntb->msi->base_addr || addr >= ntb->msi->end_addr) {
		dev_warn_once(&ntb->dev,
			      "IRQ %d: MSI Address not within the memory window (%llx, [%llx %llx])\n",
			      entry->irq, addr, ntb->msi->base_addr,
			      ntb->msi->end_addr);
		return -EFAULT;
	}

	msi_desc->addr_offset = addr - ntb->msi->base_addr;
	msi_desc->data = entry->msg.data;

	return 0;
}

static void ntb_msi_write_msg(struct msi_desc *entry, void *data)
{
	struct ntb_msi_devres *dr = data;

	WARN_ON(ntb_msi_set_desc(dr->ntb, entry, dr->msi_desc));

	if (dr->ntb->msi->desc_changed)
		dr->ntb->msi->desc_changed(dr->ntb->ctx);
}

static void ntbm_msi_callback_release(struct device *dev, void *res)
{
	struct ntb_msi_devres *dr = res;

	dr->entry->write_msi_msg = NULL;
	dr->entry->write_msi_msg_data = NULL;
}

static int ntbm_msi_setup_callback(struct ntb_dev *ntb, struct msi_desc *entry,
				   struct ntb_msi_desc *msi_desc)
{
	struct ntb_msi_devres *dr;

	dr = devres_alloc(ntbm_msi_callback_release,
			  sizeof(struct ntb_msi_devres), GFP_KERNEL);
	if (!dr)
		return -ENOMEM;

	dr->ntb = ntb;
	dr->entry = entry;
	dr->msi_desc = msi_desc;

	devres_add(&ntb->dev, dr);

	dr->entry->write_msi_msg = ntb_msi_write_msg;
	dr->entry->write_msi_msg_data = dr;

	return 0;
}

 
int ntbm_msi_request_threaded_irq(struct ntb_dev *ntb, irq_handler_t handler,
				  irq_handler_t thread_fn,
				  const char *name, void *dev_id,
				  struct ntb_msi_desc *msi_desc)
{
	struct device *dev = &ntb->pdev->dev;
	struct msi_desc *entry;
	int ret;

	if (!ntb->msi)
		return -EINVAL;

	msi_lock_descs(dev);
	msi_for_each_desc(entry, dev, MSI_DESC_ASSOCIATED) {
		if (irq_has_action(entry->irq))
			continue;

		ret = devm_request_threaded_irq(&ntb->dev, entry->irq, handler,
						thread_fn, 0, name, dev_id);
		if (ret)
			continue;

		if (ntb_msi_set_desc(ntb, entry, msi_desc)) {
			devm_free_irq(&ntb->dev, entry->irq, dev_id);
			continue;
		}

		ret = ntbm_msi_setup_callback(ntb, entry, msi_desc);
		if (ret) {
			devm_free_irq(&ntb->dev, entry->irq, dev_id);
			goto unlock;
		}

		ret = entry->irq;
		goto unlock;
	}
	ret = -ENODEV;

unlock:
	msi_unlock_descs(dev);
	return ret;
}
EXPORT_SYMBOL(ntbm_msi_request_threaded_irq);

static int ntbm_msi_callback_match(struct device *dev, void *res, void *data)
{
	struct ntb_dev *ntb = dev_ntb(dev);
	struct ntb_msi_devres *dr = res;

	return dr->ntb == ntb && dr->entry == data;
}

 
void ntbm_msi_free_irq(struct ntb_dev *ntb, unsigned int irq, void *dev_id)
{
	struct msi_desc *entry = irq_get_msi_desc(irq);

	entry->write_msi_msg = NULL;
	entry->write_msi_msg_data = NULL;

	WARN_ON(devres_destroy(&ntb->dev, ntbm_msi_callback_release,
			       ntbm_msi_callback_match, entry));

	devm_free_irq(&ntb->dev, irq, dev_id);
}
EXPORT_SYMBOL(ntbm_msi_free_irq);

 
int ntb_msi_peer_trigger(struct ntb_dev *ntb, int peer,
			 struct ntb_msi_desc *desc)
{
	int idx;

	if (!ntb->msi)
		return -EINVAL;

	idx = desc->addr_offset / sizeof(*ntb->msi->peer_mws[peer]);

	iowrite32(desc->data, &ntb->msi->peer_mws[peer][idx]);

	return 0;
}
EXPORT_SYMBOL(ntb_msi_peer_trigger);

 
int ntb_msi_peer_addr(struct ntb_dev *ntb, int peer,
		      struct ntb_msi_desc *desc,
		      phys_addr_t *msi_addr)
{
	int peer_widx = ntb_peer_mw_count(ntb) - 1 - peer;
	phys_addr_t mw_phys_addr;
	int ret;

	ret = ntb_peer_mw_get_addr(ntb, peer_widx, &mw_phys_addr, NULL);
	if (ret)
		return ret;

	if (msi_addr)
		*msi_addr = mw_phys_addr + desc->addr_offset;

	return 0;
}
EXPORT_SYMBOL(ntb_msi_peer_addr);
