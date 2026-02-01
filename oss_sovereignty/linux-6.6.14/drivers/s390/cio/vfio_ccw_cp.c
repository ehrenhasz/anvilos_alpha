
 

#include <linux/ratelimit.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/iommu.h>
#include <linux/vfio.h>
#include <asm/idals.h>

#include "vfio_ccw_cp.h"
#include "vfio_ccw_private.h"

struct page_array {
	 
	dma_addr_t		*pa_iova;
	 
	struct page		**pa_page;
	 
	int			pa_nr;
};

struct ccwchain {
	struct list_head	next;
	struct ccw1		*ch_ccw;
	 
	u64			ch_iova;
	 
	int			ch_len;
	 
	struct page_array	*ch_pa;
};

 
static int page_array_alloc(struct page_array *pa, unsigned int len)
{
	if (pa->pa_nr || pa->pa_iova)
		return -EINVAL;

	if (len == 0)
		return -EINVAL;

	pa->pa_nr = len;

	pa->pa_iova = kcalloc(len, sizeof(*pa->pa_iova), GFP_KERNEL);
	if (!pa->pa_iova)
		return -ENOMEM;

	pa->pa_page = kcalloc(len, sizeof(*pa->pa_page), GFP_KERNEL);
	if (!pa->pa_page) {
		kfree(pa->pa_iova);
		return -ENOMEM;
	}

	return 0;
}

 
static void page_array_unpin(struct page_array *pa,
			     struct vfio_device *vdev, int pa_nr, bool unaligned)
{
	int unpinned = 0, npage = 1;

	while (unpinned < pa_nr) {
		dma_addr_t *first = &pa->pa_iova[unpinned];
		dma_addr_t *last = &first[npage];

		if (unpinned + npage < pa_nr &&
		    *first + npage * PAGE_SIZE == *last &&
		    !unaligned) {
			npage++;
			continue;
		}

		vfio_unpin_pages(vdev, *first, npage);
		unpinned += npage;
		npage = 1;
	}

	pa->pa_nr = 0;
}

 
static int page_array_pin(struct page_array *pa, struct vfio_device *vdev, bool unaligned)
{
	int pinned = 0, npage = 1;
	int ret = 0;

	while (pinned < pa->pa_nr) {
		dma_addr_t *first = &pa->pa_iova[pinned];
		dma_addr_t *last = &first[npage];

		if (pinned + npage < pa->pa_nr &&
		    *first + npage * PAGE_SIZE == *last &&
		    !unaligned) {
			npage++;
			continue;
		}

		ret = vfio_pin_pages(vdev, *first, npage,
				     IOMMU_READ | IOMMU_WRITE,
				     &pa->pa_page[pinned]);
		if (ret < 0) {
			goto err_out;
		} else if (ret > 0 && ret != npage) {
			pinned += ret;
			ret = -EINVAL;
			goto err_out;
		}
		pinned += npage;
		npage = 1;
	}

	return ret;

err_out:
	page_array_unpin(pa, vdev, pinned, unaligned);
	return ret;
}

 
static void page_array_unpin_free(struct page_array *pa, struct vfio_device *vdev, bool unaligned)
{
	page_array_unpin(pa, vdev, pa->pa_nr, unaligned);
	kfree(pa->pa_page);
	kfree(pa->pa_iova);
}

static bool page_array_iova_pinned(struct page_array *pa, u64 iova, u64 length)
{
	u64 iova_pfn_start = iova >> PAGE_SHIFT;
	u64 iova_pfn_end = (iova + length - 1) >> PAGE_SHIFT;
	u64 pfn;
	int i;

	for (i = 0; i < pa->pa_nr; i++) {
		pfn = pa->pa_iova[i] >> PAGE_SHIFT;
		if (pfn >= iova_pfn_start && pfn <= iova_pfn_end)
			return true;
	}

	return false;
}
 
static inline void page_array_idal_create_words(struct page_array *pa,
						unsigned long *idaws)
{
	int i;

	 

	for (i = 0; i < pa->pa_nr; i++) {
		idaws[i] = page_to_phys(pa->pa_page[i]);

		 
		idaws[i] += pa->pa_iova[i] & (PAGE_SIZE - 1);
	}
}

static void convert_ccw0_to_ccw1(struct ccw1 *source, unsigned long len)
{
	struct ccw0 ccw0;
	struct ccw1 *pccw1 = source;
	int i;

	for (i = 0; i < len; i++) {
		ccw0 = *(struct ccw0 *)pccw1;
		if ((pccw1->cmd_code & 0x0f) == CCW_CMD_TIC) {
			pccw1->cmd_code = CCW_CMD_TIC;
			pccw1->flags = 0;
			pccw1->count = 0;
		} else {
			pccw1->cmd_code = ccw0.cmd_code;
			pccw1->flags = ccw0.flags;
			pccw1->count = ccw0.count;
		}
		pccw1->cda = ccw0.cda;
		pccw1++;
	}
}

#define idal_is_2k(_cp) (!(_cp)->orb.cmd.c64 || (_cp)->orb.cmd.i2k)

 
#define ccw_is_read(_ccw) (((_ccw)->cmd_code & 0x03) == 0x02)
#define ccw_is_read_backward(_ccw) (((_ccw)->cmd_code & 0x0F) == 0x0C)
#define ccw_is_sense(_ccw) (((_ccw)->cmd_code & 0x0F) == CCW_CMD_BASIC_SENSE)

#define ccw_is_noop(_ccw) ((_ccw)->cmd_code == CCW_CMD_NOOP)

#define ccw_is_tic(_ccw) ((_ccw)->cmd_code == CCW_CMD_TIC)

#define ccw_is_idal(_ccw) ((_ccw)->flags & CCW_FLAG_IDA)
#define ccw_is_skip(_ccw) ((_ccw)->flags & CCW_FLAG_SKIP)

#define ccw_is_chain(_ccw) ((_ccw)->flags & (CCW_FLAG_CC | CCW_FLAG_DC))

 
static inline int ccw_does_data_transfer(struct ccw1 *ccw)
{
	 
	if (ccw->count == 0)
		return 0;

	 
	if (ccw_is_noop(ccw))
		return 0;

	 
	if (!ccw_is_skip(ccw))
		return 1;

	 
	if (ccw_is_read(ccw) || ccw_is_read_backward(ccw))
		return 0;

	if (ccw_is_sense(ccw))
		return 0;

	 
	return 1;
}

 
static inline int is_cpa_within_range(u32 cpa, u32 head, int len)
{
	u32 tail = head + (len - 1) * sizeof(struct ccw1);

	return (head <= cpa && cpa <= tail);
}

static inline int is_tic_within_range(struct ccw1 *ccw, u32 head, int len)
{
	if (!ccw_is_tic(ccw))
		return 0;

	return is_cpa_within_range(ccw->cda, head, len);
}

static struct ccwchain *ccwchain_alloc(struct channel_program *cp, int len)
{
	struct ccwchain *chain;

	chain = kzalloc(sizeof(*chain), GFP_KERNEL);
	if (!chain)
		return NULL;

	chain->ch_ccw = kcalloc(len, sizeof(*chain->ch_ccw), GFP_DMA | GFP_KERNEL);
	if (!chain->ch_ccw)
		goto out_err;

	chain->ch_pa = kcalloc(len, sizeof(*chain->ch_pa), GFP_KERNEL);
	if (!chain->ch_pa)
		goto out_err;

	list_add_tail(&chain->next, &cp->ccwchain_list);

	return chain;

out_err:
	kfree(chain->ch_ccw);
	kfree(chain);
	return NULL;
}

static void ccwchain_free(struct ccwchain *chain)
{
	list_del(&chain->next);
	kfree(chain->ch_pa);
	kfree(chain->ch_ccw);
	kfree(chain);
}

 
static void ccwchain_cda_free(struct ccwchain *chain, int idx)
{
	struct ccw1 *ccw = &chain->ch_ccw[idx];

	if (ccw_is_tic(ccw))
		return;

	kfree(phys_to_virt(ccw->cda));
}

 
static int ccwchain_calc_length(u64 iova, struct channel_program *cp)
{
	struct ccw1 *ccw = cp->guest_cp;
	int cnt = 0;

	do {
		cnt++;

		 
		if (!ccw_is_chain(ccw) && !is_tic_within_range(ccw, iova, cnt))
			break;

		ccw++;
	} while (cnt < CCWCHAIN_LEN_MAX + 1);

	if (cnt == CCWCHAIN_LEN_MAX + 1)
		cnt = -EINVAL;

	return cnt;
}

static int tic_target_chain_exists(struct ccw1 *tic, struct channel_program *cp)
{
	struct ccwchain *chain;
	u32 ccw_head;

	list_for_each_entry(chain, &cp->ccwchain_list, next) {
		ccw_head = chain->ch_iova;
		if (is_cpa_within_range(tic->cda, ccw_head, chain->ch_len))
			return 1;
	}

	return 0;
}

static int ccwchain_loop_tic(struct ccwchain *chain,
			     struct channel_program *cp);

static int ccwchain_handle_ccw(u32 cda, struct channel_program *cp)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	struct ccwchain *chain;
	int len, ret;

	 
	ret = vfio_dma_rw(vdev, cda, cp->guest_cp, CCWCHAIN_LEN_MAX * sizeof(struct ccw1), false);
	if (ret)
		return ret;

	 
	if (!cp->orb.cmd.fmt)
		convert_ccw0_to_ccw1(cp->guest_cp, CCWCHAIN_LEN_MAX);

	 
	len = ccwchain_calc_length(cda, cp);
	if (len < 0)
		return len;

	 
	chain = ccwchain_alloc(cp, len);
	if (!chain)
		return -ENOMEM;

	chain->ch_len = len;
	chain->ch_iova = cda;

	 
	memcpy(chain->ch_ccw, cp->guest_cp, len * sizeof(struct ccw1));

	 
	ret = ccwchain_loop_tic(chain, cp);

	if (ret)
		ccwchain_free(chain);

	return ret;
}

 
static int ccwchain_loop_tic(struct ccwchain *chain, struct channel_program *cp)
{
	struct ccw1 *tic;
	int i, ret;

	for (i = 0; i < chain->ch_len; i++) {
		tic = &chain->ch_ccw[i];

		if (!ccw_is_tic(tic))
			continue;

		 
		if (tic_target_chain_exists(tic, cp))
			continue;

		 
		ret = ccwchain_handle_ccw(tic->cda, cp);
		if (ret)
			return ret;
	}

	return 0;
}

static int ccwchain_fetch_tic(struct ccw1 *ccw,
			      struct channel_program *cp)
{
	struct ccwchain *iter;
	u32 ccw_head;

	list_for_each_entry(iter, &cp->ccwchain_list, next) {
		ccw_head = iter->ch_iova;
		if (is_cpa_within_range(ccw->cda, ccw_head, iter->ch_len)) {
			ccw->cda = (__u32) (addr_t) (((char *)iter->ch_ccw) +
						     (ccw->cda - ccw_head));
			return 0;
		}
	}

	return -EFAULT;
}

static unsigned long *get_guest_idal(struct ccw1 *ccw,
				     struct channel_program *cp,
				     int idaw_nr)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	unsigned long *idaws;
	unsigned int *idaws_f1;
	int idal_len = idaw_nr * sizeof(*idaws);
	int idaw_size = idal_is_2k(cp) ? PAGE_SIZE / 2 : PAGE_SIZE;
	int idaw_mask = ~(idaw_size - 1);
	int i, ret;

	idaws = kcalloc(idaw_nr, sizeof(*idaws), GFP_DMA | GFP_KERNEL);
	if (!idaws)
		return ERR_PTR(-ENOMEM);

	if (ccw_is_idal(ccw)) {
		 
		ret = vfio_dma_rw(vdev, ccw->cda, idaws, idal_len, false);
		if (ret) {
			kfree(idaws);
			return ERR_PTR(ret);
		}
	} else {
		 
		if (cp->orb.cmd.c64) {
			idaws[0] = ccw->cda;
			for (i = 1; i < idaw_nr; i++)
				idaws[i] = (idaws[i - 1] + idaw_size) & idaw_mask;
		} else {
			idaws_f1 = (unsigned int *)idaws;
			idaws_f1[0] = ccw->cda;
			for (i = 1; i < idaw_nr; i++)
				idaws_f1[i] = (idaws_f1[i - 1] + idaw_size) & idaw_mask;
		}
	}

	return idaws;
}

 
static int ccw_count_idaws(struct ccw1 *ccw,
			   struct channel_program *cp)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	u64 iova;
	int size = cp->orb.cmd.c64 ? sizeof(u64) : sizeof(u32);
	int ret;
	int bytes = 1;

	if (ccw->count)
		bytes = ccw->count;

	if (ccw_is_idal(ccw)) {
		 
		 
		ret = vfio_dma_rw(vdev, ccw->cda, &iova, size, false);
		if (ret)
			return ret;

		 
		if (!cp->orb.cmd.c64)
			iova = iova >> 32;
	} else {
		iova = ccw->cda;
	}

	 
	if (!cp->orb.cmd.c64)
		return idal_2k_nr_words((void *)iova, bytes);

	 
	if (cp->orb.cmd.i2k)
		return idal_2k_nr_words((void *)iova, bytes);

	 
	return idal_nr_words((void *)iova, bytes);
}

static int ccwchain_fetch_ccw(struct ccw1 *ccw,
			      struct page_array *pa,
			      struct channel_program *cp)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	unsigned long *idaws;
	unsigned int *idaws_f1;
	int ret;
	int idaw_nr;
	int i;

	 
	idaw_nr = ccw_count_idaws(ccw, cp);
	if (idaw_nr < 0)
		return idaw_nr;

	 
	idaws = get_guest_idal(ccw, cp, idaw_nr);
	if (IS_ERR(idaws)) {
		ret = PTR_ERR(idaws);
		goto out_init;
	}

	 
	ret = page_array_alloc(pa, idaw_nr);
	if (ret < 0)
		goto out_free_idaws;

	 
	idaws_f1 = (unsigned int *)idaws;
	for (i = 0; i < idaw_nr; i++) {
		if (cp->orb.cmd.c64)
			pa->pa_iova[i] = idaws[i];
		else
			pa->pa_iova[i] = idaws_f1[i];
	}

	if (ccw_does_data_transfer(ccw)) {
		ret = page_array_pin(pa, vdev, idal_is_2k(cp));
		if (ret < 0)
			goto out_unpin;
	} else {
		pa->pa_nr = 0;
	}

	ccw->cda = (__u32) virt_to_phys(idaws);
	ccw->flags |= CCW_FLAG_IDA;

	 
	page_array_idal_create_words(pa, idaws);

	return 0;

out_unpin:
	page_array_unpin_free(pa, vdev, idal_is_2k(cp));
out_free_idaws:
	kfree(idaws);
out_init:
	ccw->cda = 0;
	return ret;
}

 
static int ccwchain_fetch_one(struct ccw1 *ccw,
			      struct page_array *pa,
			      struct channel_program *cp)

{
	if (ccw_is_tic(ccw))
		return ccwchain_fetch_tic(ccw, cp);

	return ccwchain_fetch_ccw(ccw, pa, cp);
}

 
int cp_init(struct channel_program *cp, union orb *orb)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	 
	static DEFINE_RATELIMIT_STATE(ratelimit_state, 5 * HZ, 1);
	int ret;

	 
	if (cp->initialized)
		return -EBUSY;

	 
	if (!orb->cmd.pfch && __ratelimit(&ratelimit_state))
		dev_warn(
			vdev->dev,
			"Prefetching channel program even though prefetch not specified in ORB");

	INIT_LIST_HEAD(&cp->ccwchain_list);
	memcpy(&cp->orb, orb, sizeof(*orb));

	 
	ret = ccwchain_handle_ccw(orb->cmd.cpa, cp);

	if (!ret)
		cp->initialized = true;

	return ret;
}


 
void cp_free(struct channel_program *cp)
{
	struct vfio_device *vdev =
		&container_of(cp, struct vfio_ccw_private, cp)->vdev;
	struct ccwchain *chain, *temp;
	int i;

	if (!cp->initialized)
		return;

	cp->initialized = false;
	list_for_each_entry_safe(chain, temp, &cp->ccwchain_list, next) {
		for (i = 0; i < chain->ch_len; i++) {
			page_array_unpin_free(&chain->ch_pa[i], vdev, idal_is_2k(cp));
			ccwchain_cda_free(chain, i);
		}
		ccwchain_free(chain);
	}
}

 
int cp_prefetch(struct channel_program *cp)
{
	struct ccwchain *chain;
	struct ccw1 *ccw;
	struct page_array *pa;
	int len, idx, ret;

	 
	if (!cp->initialized)
		return -EINVAL;

	list_for_each_entry(chain, &cp->ccwchain_list, next) {
		len = chain->ch_len;
		for (idx = 0; idx < len; idx++) {
			ccw = &chain->ch_ccw[idx];
			pa = &chain->ch_pa[idx];

			ret = ccwchain_fetch_one(ccw, pa, cp);
			if (ret)
				goto out_err;
		}
	}

	return 0;
out_err:
	 
	chain->ch_len = idx;
	list_for_each_entry_continue(chain, &cp->ccwchain_list, next) {
		chain->ch_len = 0;
	}
	return ret;
}

 
union orb *cp_get_orb(struct channel_program *cp, struct subchannel *sch)
{
	union orb *orb;
	struct ccwchain *chain;
	struct ccw1 *cpa;

	 
	if (!cp->initialized)
		return NULL;

	orb = &cp->orb;

	orb->cmd.intparm = (u32)virt_to_phys(sch);
	orb->cmd.fmt = 1;

	 
	if (!orb->cmd.c64)
		orb->cmd.i2k = 1;
	orb->cmd.c64 = 1;

	if (orb->cmd.lpm == 0)
		orb->cmd.lpm = sch->lpm;

	chain = list_first_entry(&cp->ccwchain_list, struct ccwchain, next);
	cpa = chain->ch_ccw;
	orb->cmd.cpa = (__u32)virt_to_phys(cpa);

	return orb;
}

 
void cp_update_scsw(struct channel_program *cp, union scsw *scsw)
{
	struct ccwchain *chain;
	u32 cpa = scsw->cmd.cpa;
	u32 ccw_head;

	if (!cp->initialized)
		return;

	 
	list_for_each_entry(chain, &cp->ccwchain_list, next) {
		ccw_head = (u32)(u64)chain->ch_ccw;
		 
		if (is_cpa_within_range(cpa, ccw_head, chain->ch_len + 1)) {
			 
			cpa = chain->ch_iova + (cpa - ccw_head);
			break;
		}
	}

	scsw->cmd.cpa = cpa;
}

 
bool cp_iova_pinned(struct channel_program *cp, u64 iova, u64 length)
{
	struct ccwchain *chain;
	int i;

	if (!cp->initialized)
		return false;

	list_for_each_entry(chain, &cp->ccwchain_list, next) {
		for (i = 0; i < chain->ch_len; i++)
			if (page_array_iova_pinned(&chain->ch_pa[i], iova, length))
				return true;
	}

	return false;
}
