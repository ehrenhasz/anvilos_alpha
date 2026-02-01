 

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/highmem.h>
#include <linux/timer.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/edac.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include "edac_mc.h"
#include "edac_module.h"
#include <ras/ras_event.h>

#ifdef CONFIG_EDAC_ATOMIC_SCRUB
#include <asm/edac.h>
#else
#define edac_atomic_scrub(va, size) do { } while (0)
#endif

int edac_op_state = EDAC_OPSTATE_INVAL;
EXPORT_SYMBOL_GPL(edac_op_state);

 
static DEFINE_MUTEX(mem_ctls_mutex);
static LIST_HEAD(mc_devices);

 
static const char *edac_mc_owner;

static struct mem_ctl_info *error_desc_to_mci(struct edac_raw_error_desc *e)
{
	return container_of(e, struct mem_ctl_info, error_desc);
}

unsigned int edac_dimm_info_location(struct dimm_info *dimm, char *buf,
				     unsigned int len)
{
	struct mem_ctl_info *mci = dimm->mci;
	int i, n, count = 0;
	char *p = buf;

	for (i = 0; i < mci->n_layers; i++) {
		n = scnprintf(p, len, "%s %d ",
			      edac_layer_name[mci->layers[i].type],
			      dimm->location[i]);
		p += n;
		len -= n;
		count += n;
	}

	return count;
}

#ifdef CONFIG_EDAC_DEBUG

static void edac_mc_dump_channel(struct rank_info *chan)
{
	edac_dbg(4, "  channel->chan_idx = %d\n", chan->chan_idx);
	edac_dbg(4, "    channel = %p\n", chan);
	edac_dbg(4, "    channel->csrow = %p\n", chan->csrow);
	edac_dbg(4, "    channel->dimm = %p\n", chan->dimm);
}

static void edac_mc_dump_dimm(struct dimm_info *dimm)
{
	char location[80];

	if (!dimm->nr_pages)
		return;

	edac_dimm_info_location(dimm, location, sizeof(location));

	edac_dbg(4, "%s%i: %smapped as virtual row %d, chan %d\n",
		 dimm->mci->csbased ? "rank" : "dimm",
		 dimm->idx, location, dimm->csrow, dimm->cschannel);
	edac_dbg(4, "  dimm = %p\n", dimm);
	edac_dbg(4, "  dimm->label = '%s'\n", dimm->label);
	edac_dbg(4, "  dimm->nr_pages = 0x%x\n", dimm->nr_pages);
	edac_dbg(4, "  dimm->grain = %d\n", dimm->grain);
}

static void edac_mc_dump_csrow(struct csrow_info *csrow)
{
	edac_dbg(4, "csrow->csrow_idx = %d\n", csrow->csrow_idx);
	edac_dbg(4, "  csrow = %p\n", csrow);
	edac_dbg(4, "  csrow->first_page = 0x%lx\n", csrow->first_page);
	edac_dbg(4, "  csrow->last_page = 0x%lx\n", csrow->last_page);
	edac_dbg(4, "  csrow->page_mask = 0x%lx\n", csrow->page_mask);
	edac_dbg(4, "  csrow->nr_channels = %d\n", csrow->nr_channels);
	edac_dbg(4, "  csrow->channels = %p\n", csrow->channels);
	edac_dbg(4, "  csrow->mci = %p\n", csrow->mci);
}

static void edac_mc_dump_mci(struct mem_ctl_info *mci)
{
	edac_dbg(3, "\tmci = %p\n", mci);
	edac_dbg(3, "\tmci->mtype_cap = %lx\n", mci->mtype_cap);
	edac_dbg(3, "\tmci->edac_ctl_cap = %lx\n", mci->edac_ctl_cap);
	edac_dbg(3, "\tmci->edac_cap = %lx\n", mci->edac_cap);
	edac_dbg(4, "\tmci->edac_check = %p\n", mci->edac_check);
	edac_dbg(3, "\tmci->nr_csrows = %d, csrows = %p\n",
		 mci->nr_csrows, mci->csrows);
	edac_dbg(3, "\tmci->nr_dimms = %d, dimms = %p\n",
		 mci->tot_dimms, mci->dimms);
	edac_dbg(3, "\tdev = %p\n", mci->pdev);
	edac_dbg(3, "\tmod_name:ctl_name = %s:%s\n",
		 mci->mod_name, mci->ctl_name);
	edac_dbg(3, "\tpvt_info = %p\n\n", mci->pvt_info);
}

#endif				 

const char * const edac_mem_types[] = {
	[MEM_EMPTY]	= "Empty",
	[MEM_RESERVED]	= "Reserved",
	[MEM_UNKNOWN]	= "Unknown",
	[MEM_FPM]	= "FPM",
	[MEM_EDO]	= "EDO",
	[MEM_BEDO]	= "BEDO",
	[MEM_SDR]	= "Unbuffered-SDR",
	[MEM_RDR]	= "Registered-SDR",
	[MEM_DDR]	= "Unbuffered-DDR",
	[MEM_RDDR]	= "Registered-DDR",
	[MEM_RMBS]	= "RMBS",
	[MEM_DDR2]	= "Unbuffered-DDR2",
	[MEM_FB_DDR2]	= "FullyBuffered-DDR2",
	[MEM_RDDR2]	= "Registered-DDR2",
	[MEM_XDR]	= "XDR",
	[MEM_DDR3]	= "Unbuffered-DDR3",
	[MEM_RDDR3]	= "Registered-DDR3",
	[MEM_LRDDR3]	= "Load-Reduced-DDR3-RAM",
	[MEM_LPDDR3]	= "Low-Power-DDR3-RAM",
	[MEM_DDR4]	= "Unbuffered-DDR4",
	[MEM_RDDR4]	= "Registered-DDR4",
	[MEM_LPDDR4]	= "Low-Power-DDR4-RAM",
	[MEM_LRDDR4]	= "Load-Reduced-DDR4-RAM",
	[MEM_DDR5]	= "Unbuffered-DDR5",
	[MEM_RDDR5]	= "Registered-DDR5",
	[MEM_LRDDR5]	= "Load-Reduced-DDR5-RAM",
	[MEM_NVDIMM]	= "Non-volatile-RAM",
	[MEM_WIO2]	= "Wide-IO-2",
	[MEM_HBM2]	= "High-bandwidth-memory-Gen2",
};
EXPORT_SYMBOL_GPL(edac_mem_types);

static void _edac_mc_free(struct mem_ctl_info *mci)
{
	put_device(&mci->dev);
}

static void mci_release(struct device *dev)
{
	struct mem_ctl_info *mci = container_of(dev, struct mem_ctl_info, dev);
	struct csrow_info *csr;
	int i, chn, row;

	if (mci->dimms) {
		for (i = 0; i < mci->tot_dimms; i++)
			kfree(mci->dimms[i]);
		kfree(mci->dimms);
	}

	if (mci->csrows) {
		for (row = 0; row < mci->nr_csrows; row++) {
			csr = mci->csrows[row];
			if (!csr)
				continue;

			if (csr->channels) {
				for (chn = 0; chn < mci->num_cschannel; chn++)
					kfree(csr->channels[chn]);
				kfree(csr->channels);
			}
			kfree(csr);
		}
		kfree(mci->csrows);
	}
	kfree(mci->pvt_info);
	kfree(mci->layers);
	kfree(mci);
}

static int edac_mc_alloc_csrows(struct mem_ctl_info *mci)
{
	unsigned int tot_channels = mci->num_cschannel;
	unsigned int tot_csrows = mci->nr_csrows;
	unsigned int row, chn;

	 
	mci->csrows = kcalloc(tot_csrows, sizeof(*mci->csrows), GFP_KERNEL);
	if (!mci->csrows)
		return -ENOMEM;

	for (row = 0; row < tot_csrows; row++) {
		struct csrow_info *csr;

		csr = kzalloc(sizeof(**mci->csrows), GFP_KERNEL);
		if (!csr)
			return -ENOMEM;

		mci->csrows[row] = csr;
		csr->csrow_idx = row;
		csr->mci = mci;
		csr->nr_channels = tot_channels;
		csr->channels = kcalloc(tot_channels, sizeof(*csr->channels),
					GFP_KERNEL);
		if (!csr->channels)
			return -ENOMEM;

		for (chn = 0; chn < tot_channels; chn++) {
			struct rank_info *chan;

			chan = kzalloc(sizeof(**csr->channels), GFP_KERNEL);
			if (!chan)
				return -ENOMEM;

			csr->channels[chn] = chan;
			chan->chan_idx = chn;
			chan->csrow = csr;
		}
	}

	return 0;
}

static int edac_mc_alloc_dimms(struct mem_ctl_info *mci)
{
	unsigned int pos[EDAC_MAX_LAYERS];
	unsigned int row, chn, idx;
	int layer;
	void *p;

	 
	mci->dimms  = kcalloc(mci->tot_dimms, sizeof(*mci->dimms), GFP_KERNEL);
	if (!mci->dimms)
		return -ENOMEM;

	memset(&pos, 0, sizeof(pos));
	row = 0;
	chn = 0;
	for (idx = 0; idx < mci->tot_dimms; idx++) {
		struct dimm_info *dimm;
		struct rank_info *chan;
		int n, len;

		chan = mci->csrows[row]->channels[chn];

		dimm = kzalloc(sizeof(**mci->dimms), GFP_KERNEL);
		if (!dimm)
			return -ENOMEM;
		mci->dimms[idx] = dimm;
		dimm->mci = mci;
		dimm->idx = idx;

		 
		len = sizeof(dimm->label);
		p = dimm->label;
		n = scnprintf(p, len, "mc#%u", mci->mc_idx);
		p += n;
		len -= n;
		for (layer = 0; layer < mci->n_layers; layer++) {
			n = scnprintf(p, len, "%s#%u",
				      edac_layer_name[mci->layers[layer].type],
				      pos[layer]);
			p += n;
			len -= n;
			dimm->location[layer] = pos[layer];
		}

		 
		chan->dimm = dimm;
		dimm->csrow = row;
		dimm->cschannel = chn;

		 
		if (mci->layers[0].is_virt_csrow) {
			chn++;
			if (chn == mci->num_cschannel) {
				chn = 0;
				row++;
			}
		} else {
			row++;
			if (row == mci->nr_csrows) {
				row = 0;
				chn++;
			}
		}

		 
		for (layer = mci->n_layers - 1; layer >= 0; layer--) {
			pos[layer]++;
			if (pos[layer] < mci->layers[layer].size)
				break;
			pos[layer] = 0;
		}
	}

	return 0;
}

struct mem_ctl_info *edac_mc_alloc(unsigned int mc_num,
				   unsigned int n_layers,
				   struct edac_mc_layer *layers,
				   unsigned int sz_pvt)
{
	struct mem_ctl_info *mci;
	struct edac_mc_layer *layer;
	unsigned int idx, tot_dimms = 1;
	unsigned int tot_csrows = 1, tot_channels = 1;
	bool per_rank = false;

	if (WARN_ON(n_layers > EDAC_MAX_LAYERS || n_layers == 0))
		return NULL;

	 
	for (idx = 0; idx < n_layers; idx++) {
		tot_dimms *= layers[idx].size;

		if (layers[idx].is_virt_csrow)
			tot_csrows *= layers[idx].size;
		else
			tot_channels *= layers[idx].size;

		if (layers[idx].type == EDAC_MC_LAYER_CHIP_SELECT)
			per_rank = true;
	}

	mci = kzalloc(sizeof(struct mem_ctl_info), GFP_KERNEL);
	if (!mci)
		return NULL;

	mci->layers = kcalloc(n_layers, sizeof(struct edac_mc_layer), GFP_KERNEL);
	if (!mci->layers)
		goto error;

	mci->pvt_info = kzalloc(sz_pvt, GFP_KERNEL);
	if (!mci->pvt_info)
		goto error;

	mci->dev.release = mci_release;
	device_initialize(&mci->dev);

	 
	mci->mc_idx = mc_num;
	mci->tot_dimms = tot_dimms;
	mci->n_layers = n_layers;
	memcpy(mci->layers, layers, sizeof(*layer) * n_layers);
	mci->nr_csrows = tot_csrows;
	mci->num_cschannel = tot_channels;
	mci->csbased = per_rank;

	if (edac_mc_alloc_csrows(mci))
		goto error;

	if (edac_mc_alloc_dimms(mci))
		goto error;

	mci->op_state = OP_ALLOC;

	return mci;

error:
	_edac_mc_free(mci);

	return NULL;
}
EXPORT_SYMBOL_GPL(edac_mc_alloc);

void edac_mc_free(struct mem_ctl_info *mci)
{
	edac_dbg(1, "\n");

	_edac_mc_free(mci);
}
EXPORT_SYMBOL_GPL(edac_mc_free);

bool edac_has_mcs(void)
{
	bool ret;

	mutex_lock(&mem_ctls_mutex);

	ret = list_empty(&mc_devices);

	mutex_unlock(&mem_ctls_mutex);

	return !ret;
}
EXPORT_SYMBOL_GPL(edac_has_mcs);

 
static struct mem_ctl_info *__find_mci_by_dev(struct device *dev)
{
	struct mem_ctl_info *mci;
	struct list_head *item;

	edac_dbg(3, "\n");

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		if (mci->pdev == dev)
			return mci;
	}

	return NULL;
}

 
struct mem_ctl_info *find_mci_by_dev(struct device *dev)
{
	struct mem_ctl_info *ret;

	mutex_lock(&mem_ctls_mutex);
	ret = __find_mci_by_dev(dev);
	mutex_unlock(&mem_ctls_mutex);

	return ret;
}
EXPORT_SYMBOL_GPL(find_mci_by_dev);

 
static void edac_mc_workq_function(struct work_struct *work_req)
{
	struct delayed_work *d_work = to_delayed_work(work_req);
	struct mem_ctl_info *mci = to_edac_mem_ctl_work(d_work);

	mutex_lock(&mem_ctls_mutex);

	if (mci->op_state != OP_RUNNING_POLL) {
		mutex_unlock(&mem_ctls_mutex);
		return;
	}

	if (edac_op_state == EDAC_OPSTATE_POLL)
		mci->edac_check(mci);

	mutex_unlock(&mem_ctls_mutex);

	 
	edac_queue_work(&mci->work, msecs_to_jiffies(edac_mc_get_poll_msec()));
}

 
void edac_mc_reset_delay_period(unsigned long value)
{
	struct mem_ctl_info *mci;
	struct list_head *item;

	mutex_lock(&mem_ctls_mutex);

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);

		if (mci->op_state == OP_RUNNING_POLL)
			edac_mod_work(&mci->work, value);
	}
	mutex_unlock(&mem_ctls_mutex);
}



 
static int add_mc_to_global_list(struct mem_ctl_info *mci)
{
	struct list_head *item, *insert_before;
	struct mem_ctl_info *p;

	insert_before = &mc_devices;

	p = __find_mci_by_dev(mci->pdev);
	if (unlikely(p != NULL))
		goto fail0;

	list_for_each(item, &mc_devices) {
		p = list_entry(item, struct mem_ctl_info, link);

		if (p->mc_idx >= mci->mc_idx) {
			if (unlikely(p->mc_idx == mci->mc_idx))
				goto fail1;

			insert_before = item;
			break;
		}
	}

	list_add_tail_rcu(&mci->link, insert_before);
	return 0;

fail0:
	edac_printk(KERN_WARNING, EDAC_MC,
		"%s (%s) %s %s already assigned %d\n", dev_name(p->pdev),
		edac_dev_name(mci), p->mod_name, p->ctl_name, p->mc_idx);
	return 1;

fail1:
	edac_printk(KERN_WARNING, EDAC_MC,
		"bug in low-level driver: attempt to assign\n"
		"    duplicate mc_idx %d in %s()\n", p->mc_idx, __func__);
	return 1;
}

static int del_mc_from_global_list(struct mem_ctl_info *mci)
{
	list_del_rcu(&mci->link);

	 
	synchronize_rcu();
	INIT_LIST_HEAD(&mci->link);

	return list_empty(&mc_devices);
}

struct mem_ctl_info *edac_mc_find(int idx)
{
	struct mem_ctl_info *mci;
	struct list_head *item;

	mutex_lock(&mem_ctls_mutex);

	list_for_each(item, &mc_devices) {
		mci = list_entry(item, struct mem_ctl_info, link);
		if (mci->mc_idx == idx)
			goto unlock;
	}

	mci = NULL;
unlock:
	mutex_unlock(&mem_ctls_mutex);
	return mci;
}
EXPORT_SYMBOL(edac_mc_find);

const char *edac_get_owner(void)
{
	return edac_mc_owner;
}
EXPORT_SYMBOL_GPL(edac_get_owner);

 
int edac_mc_add_mc_with_groups(struct mem_ctl_info *mci,
			       const struct attribute_group **groups)
{
	int ret = -EINVAL;
	edac_dbg(0, "\n");

#ifdef CONFIG_EDAC_DEBUG
	if (edac_debug_level >= 3)
		edac_mc_dump_mci(mci);

	if (edac_debug_level >= 4) {
		struct dimm_info *dimm;
		int i;

		for (i = 0; i < mci->nr_csrows; i++) {
			struct csrow_info *csrow = mci->csrows[i];
			u32 nr_pages = 0;
			int j;

			for (j = 0; j < csrow->nr_channels; j++)
				nr_pages += csrow->channels[j]->dimm->nr_pages;
			if (!nr_pages)
				continue;
			edac_mc_dump_csrow(csrow);
			for (j = 0; j < csrow->nr_channels; j++)
				if (csrow->channels[j]->dimm->nr_pages)
					edac_mc_dump_channel(csrow->channels[j]);
		}

		mci_for_each_dimm(mci, dimm)
			edac_mc_dump_dimm(dimm);
	}
#endif
	mutex_lock(&mem_ctls_mutex);

	if (edac_mc_owner && edac_mc_owner != mci->mod_name) {
		ret = -EPERM;
		goto fail0;
	}

	if (add_mc_to_global_list(mci))
		goto fail0;

	 
	mci->start_time = jiffies;

	mci->bus = edac_get_sysfs_subsys();

	if (edac_create_sysfs_mci_device(mci, groups)) {
		edac_mc_printk(mci, KERN_WARNING,
			"failed to create sysfs device\n");
		goto fail1;
	}

	if (mci->edac_check) {
		mci->op_state = OP_RUNNING_POLL;

		INIT_DELAYED_WORK(&mci->work, edac_mc_workq_function);
		edac_queue_work(&mci->work, msecs_to_jiffies(edac_mc_get_poll_msec()));

	} else {
		mci->op_state = OP_RUNNING_INTERRUPT;
	}

	 
	edac_mc_printk(mci, KERN_INFO,
		"Giving out device to module %s controller %s: DEV %s (%s)\n",
		mci->mod_name, mci->ctl_name, mci->dev_name,
		edac_op_state_to_string(mci->op_state));

	edac_mc_owner = mci->mod_name;

	mutex_unlock(&mem_ctls_mutex);
	return 0;

fail1:
	del_mc_from_global_list(mci);

fail0:
	mutex_unlock(&mem_ctls_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(edac_mc_add_mc_with_groups);

struct mem_ctl_info *edac_mc_del_mc(struct device *dev)
{
	struct mem_ctl_info *mci;

	edac_dbg(0, "\n");

	mutex_lock(&mem_ctls_mutex);

	 
	mci = __find_mci_by_dev(dev);
	if (mci == NULL) {
		mutex_unlock(&mem_ctls_mutex);
		return NULL;
	}

	 
	mci->op_state = OP_OFFLINE;

	if (del_mc_from_global_list(mci))
		edac_mc_owner = NULL;

	mutex_unlock(&mem_ctls_mutex);

	if (mci->edac_check)
		edac_stop_work(&mci->work);

	 
	edac_remove_sysfs_mci_device(mci);

	edac_printk(KERN_INFO, EDAC_MC,
		"Removed device %d for %s %s: DEV %s\n", mci->mc_idx,
		mci->mod_name, mci->ctl_name, edac_dev_name(mci));

	return mci;
}
EXPORT_SYMBOL_GPL(edac_mc_del_mc);

static void edac_mc_scrub_block(unsigned long page, unsigned long offset,
				u32 size)
{
	struct page *pg;
	void *virt_addr;
	unsigned long flags = 0;

	edac_dbg(3, "\n");

	 
	if (!pfn_valid(page))
		return;

	 
	pg = pfn_to_page(page);

	if (PageHighMem(pg))
		local_irq_save(flags);

	virt_addr = kmap_atomic(pg);

	 
	edac_atomic_scrub(virt_addr + offset, size);

	 
	kunmap_atomic(virt_addr);

	if (PageHighMem(pg))
		local_irq_restore(flags);
}

 
int edac_mc_find_csrow_by_page(struct mem_ctl_info *mci, unsigned long page)
{
	struct csrow_info **csrows = mci->csrows;
	int row, i, j, n;

	edac_dbg(1, "MC%d: 0x%lx\n", mci->mc_idx, page);
	row = -1;

	for (i = 0; i < mci->nr_csrows; i++) {
		struct csrow_info *csrow = csrows[i];
		n = 0;
		for (j = 0; j < csrow->nr_channels; j++) {
			struct dimm_info *dimm = csrow->channels[j]->dimm;
			n += dimm->nr_pages;
		}
		if (n == 0)
			continue;

		edac_dbg(3, "MC%d: first(0x%lx) page(0x%lx) last(0x%lx) mask(0x%lx)\n",
			 mci->mc_idx,
			 csrow->first_page, page, csrow->last_page,
			 csrow->page_mask);

		if ((page >= csrow->first_page) &&
		    (page <= csrow->last_page) &&
		    ((page & csrow->page_mask) ==
		     (csrow->first_page & csrow->page_mask))) {
			row = i;
			break;
		}
	}

	if (row == -1)
		edac_mc_printk(mci, KERN_ERR,
			"could not look up page error address %lx\n",
			(unsigned long)page);

	return row;
}
EXPORT_SYMBOL_GPL(edac_mc_find_csrow_by_page);

const char *edac_layer_name[] = {
	[EDAC_MC_LAYER_BRANCH] = "branch",
	[EDAC_MC_LAYER_CHANNEL] = "channel",
	[EDAC_MC_LAYER_SLOT] = "slot",
	[EDAC_MC_LAYER_CHIP_SELECT] = "csrow",
	[EDAC_MC_LAYER_ALL_MEM] = "memory",
};
EXPORT_SYMBOL_GPL(edac_layer_name);

static void edac_inc_ce_error(struct edac_raw_error_desc *e)
{
	int pos[EDAC_MAX_LAYERS] = { e->top_layer, e->mid_layer, e->low_layer };
	struct mem_ctl_info *mci = error_desc_to_mci(e);
	struct dimm_info *dimm = edac_get_dimm(mci, pos[0], pos[1], pos[2]);

	mci->ce_mc += e->error_count;

	if (dimm)
		dimm->ce_count += e->error_count;
	else
		mci->ce_noinfo_count += e->error_count;
}

static void edac_inc_ue_error(struct edac_raw_error_desc *e)
{
	int pos[EDAC_MAX_LAYERS] = { e->top_layer, e->mid_layer, e->low_layer };
	struct mem_ctl_info *mci = error_desc_to_mci(e);
	struct dimm_info *dimm = edac_get_dimm(mci, pos[0], pos[1], pos[2]);

	mci->ue_mc += e->error_count;

	if (dimm)
		dimm->ue_count += e->error_count;
	else
		mci->ue_noinfo_count += e->error_count;
}

static void edac_ce_error(struct edac_raw_error_desc *e)
{
	struct mem_ctl_info *mci = error_desc_to_mci(e);
	unsigned long remapped_page;

	if (edac_mc_get_log_ce()) {
		edac_mc_printk(mci, KERN_WARNING,
			"%d CE %s%son %s (%s page:0x%lx offset:0x%lx grain:%ld syndrome:0x%lx%s%s)\n",
			e->error_count, e->msg,
			*e->msg ? " " : "",
			e->label, e->location, e->page_frame_number, e->offset_in_page,
			e->grain, e->syndrome,
			*e->other_detail ? " - " : "",
			e->other_detail);
	}

	edac_inc_ce_error(e);

	if (mci->scrub_mode == SCRUB_SW_SRC) {
		 
		remapped_page = mci->ctl_page_to_phys ?
			mci->ctl_page_to_phys(mci, e->page_frame_number) :
			e->page_frame_number;

		edac_mc_scrub_block(remapped_page, e->offset_in_page, e->grain);
	}
}

static void edac_ue_error(struct edac_raw_error_desc *e)
{
	struct mem_ctl_info *mci = error_desc_to_mci(e);

	if (edac_mc_get_log_ue()) {
		edac_mc_printk(mci, KERN_WARNING,
			"%d UE %s%son %s (%s page:0x%lx offset:0x%lx grain:%ld%s%s)\n",
			e->error_count, e->msg,
			*e->msg ? " " : "",
			e->label, e->location, e->page_frame_number, e->offset_in_page,
			e->grain,
			*e->other_detail ? " - " : "",
			e->other_detail);
	}

	edac_inc_ue_error(e);

	if (edac_mc_get_panic_on_ue()) {
		panic("UE %s%son %s (%s page:0x%lx offset:0x%lx grain:%ld%s%s)\n",
			e->msg,
			*e->msg ? " " : "",
			e->label, e->location, e->page_frame_number, e->offset_in_page,
			e->grain,
			*e->other_detail ? " - " : "",
			e->other_detail);
	}
}

static void edac_inc_csrow(struct edac_raw_error_desc *e, int row, int chan)
{
	struct mem_ctl_info *mci = error_desc_to_mci(e);
	enum hw_event_mc_err_type type = e->type;
	u16 count = e->error_count;

	if (row < 0)
		return;

	edac_dbg(4, "csrow/channel to increment: (%d,%d)\n", row, chan);

	if (type == HW_EVENT_ERR_CORRECTED) {
		mci->csrows[row]->ce_count += count;
		if (chan >= 0)
			mci->csrows[row]->channels[chan]->ce_count += count;
	} else {
		mci->csrows[row]->ue_count += count;
	}
}

void edac_raw_mc_handle_error(struct edac_raw_error_desc *e)
{
	struct mem_ctl_info *mci = error_desc_to_mci(e);
	u8 grain_bits;

	 
	if (WARN_ON_ONCE(!e->grain))
		e->grain = 1;

	grain_bits = fls_long(e->grain - 1);

	 
	if (IS_ENABLED(CONFIG_RAS))
		trace_mc_event(e->type, e->msg, e->label, e->error_count,
			       mci->mc_idx, e->top_layer, e->mid_layer,
			       e->low_layer,
			       (e->page_frame_number << PAGE_SHIFT) | e->offset_in_page,
			       grain_bits, e->syndrome, e->other_detail);

	if (e->type == HW_EVENT_ERR_CORRECTED)
		edac_ce_error(e);
	else
		edac_ue_error(e);
}
EXPORT_SYMBOL_GPL(edac_raw_mc_handle_error);

void edac_mc_handle_error(const enum hw_event_mc_err_type type,
			  struct mem_ctl_info *mci,
			  const u16 error_count,
			  const unsigned long page_frame_number,
			  const unsigned long offset_in_page,
			  const unsigned long syndrome,
			  const int top_layer,
			  const int mid_layer,
			  const int low_layer,
			  const char *msg,
			  const char *other_detail)
{
	struct dimm_info *dimm;
	char *p, *end;
	int row = -1, chan = -1;
	int pos[EDAC_MAX_LAYERS] = { top_layer, mid_layer, low_layer };
	int i, n_labels = 0;
	struct edac_raw_error_desc *e = &mci->error_desc;
	bool any_memory = true;
	const char *prefix;

	edac_dbg(3, "MC%d\n", mci->mc_idx);

	 
	memset(e, 0, sizeof (*e));
	e->error_count = error_count;
	e->type = type;
	e->top_layer = top_layer;
	e->mid_layer = mid_layer;
	e->low_layer = low_layer;
	e->page_frame_number = page_frame_number;
	e->offset_in_page = offset_in_page;
	e->syndrome = syndrome;
	 
	e->msg = msg ?: "";
	e->other_detail = other_detail ?: "";

	 
	for (i = 0; i < mci->n_layers; i++) {
		if (pos[i] >= (int)mci->layers[i].size) {

			edac_mc_printk(mci, KERN_ERR,
				       "INTERNAL ERROR: %s value is out of range (%d >= %d)\n",
				       edac_layer_name[mci->layers[i].type],
				       pos[i], mci->layers[i].size);
			 
			pos[i] = -1;
		}
		if (pos[i] >= 0)
			any_memory = false;
	}

	 
	p = e->label;
	*p = '\0';
	end = p + sizeof(e->label);
	prefix = "";

	mci_for_each_dimm(mci, dimm) {
		if (top_layer >= 0 && top_layer != dimm->location[0])
			continue;
		if (mid_layer >= 0 && mid_layer != dimm->location[1])
			continue;
		if (low_layer >= 0 && low_layer != dimm->location[2])
			continue;

		 
		if (dimm->grain > e->grain)
			e->grain = dimm->grain;

		 
		if (!dimm->nr_pages)
			continue;

		n_labels++;
		if (n_labels > EDAC_MAX_LABELS) {
			p = e->label;
			*p = '\0';
		} else {
			p += scnprintf(p, end - p, "%s%s", prefix, dimm->label);
			prefix = OTHER_LABEL;
		}

		 
		edac_dbg(4, "%s csrows map: (%d,%d)\n",
			mci->csbased ? "rank" : "dimm",
			dimm->csrow, dimm->cschannel);
		if (row == -1)
			row = dimm->csrow;
		else if (row >= 0 && row != dimm->csrow)
			row = -2;

		if (chan == -1)
			chan = dimm->cschannel;
		else if (chan >= 0 && chan != dimm->cschannel)
			chan = -2;
	}

	if (any_memory)
		strscpy(e->label, "any memory", sizeof(e->label));
	else if (!*e->label)
		strscpy(e->label, "unknown memory", sizeof(e->label));

	edac_inc_csrow(e, row, chan);

	 
	p = e->location;
	end = p + sizeof(e->location);
	prefix = "";

	for (i = 0; i < mci->n_layers; i++) {
		if (pos[i] < 0)
			continue;

		p += scnprintf(p, end - p, "%s%s:%d", prefix,
			       edac_layer_name[mci->layers[i].type], pos[i]);
		prefix = " ";
	}

	edac_raw_mc_handle_error(e);
}
EXPORT_SYMBOL_GPL(edac_mc_handle_error);
