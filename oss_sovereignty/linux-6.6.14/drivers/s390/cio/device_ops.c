
 
#include <linux/export.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/completion.h>

#include <asm/ccwdev.h>
#include <asm/idals.h>
#include <asm/chpid.h>
#include <asm/fcx.h>

#include "cio.h"
#include "cio_debug.h"
#include "css.h"
#include "chsc.h"
#include "device.h"
#include "chp.h"

 
int ccw_device_set_options_mask(struct ccw_device *cdev, unsigned long flags)
{
        
	if ((flags & CCWDEV_EARLY_NOTIFICATION) &&
	    (flags & CCWDEV_REPORT_ALL))
		return -EINVAL;
	cdev->private->options.fast = (flags & CCWDEV_EARLY_NOTIFICATION) != 0;
	cdev->private->options.repall = (flags & CCWDEV_REPORT_ALL) != 0;
	cdev->private->options.pgroup = (flags & CCWDEV_DO_PATHGROUP) != 0;
	cdev->private->options.force = (flags & CCWDEV_ALLOW_FORCE) != 0;
	cdev->private->options.mpath = (flags & CCWDEV_DO_MULTIPATH) != 0;
	return 0;
}

 
int ccw_device_set_options(struct ccw_device *cdev, unsigned long flags)
{
        
	if (((flags & CCWDEV_EARLY_NOTIFICATION) &&
	    (flags & CCWDEV_REPORT_ALL)) ||
	    ((flags & CCWDEV_EARLY_NOTIFICATION) &&
	     cdev->private->options.repall) ||
	    ((flags & CCWDEV_REPORT_ALL) &&
	     cdev->private->options.fast))
		return -EINVAL;
	cdev->private->options.fast |= (flags & CCWDEV_EARLY_NOTIFICATION) != 0;
	cdev->private->options.repall |= (flags & CCWDEV_REPORT_ALL) != 0;
	cdev->private->options.pgroup |= (flags & CCWDEV_DO_PATHGROUP) != 0;
	cdev->private->options.force |= (flags & CCWDEV_ALLOW_FORCE) != 0;
	cdev->private->options.mpath |= (flags & CCWDEV_DO_MULTIPATH) != 0;
	return 0;
}

 
void ccw_device_clear_options(struct ccw_device *cdev, unsigned long flags)
{
	cdev->private->options.fast &= (flags & CCWDEV_EARLY_NOTIFICATION) == 0;
	cdev->private->options.repall &= (flags & CCWDEV_REPORT_ALL) == 0;
	cdev->private->options.pgroup &= (flags & CCWDEV_DO_PATHGROUP) == 0;
	cdev->private->options.force &= (flags & CCWDEV_ALLOW_FORCE) == 0;
	cdev->private->options.mpath &= (flags & CCWDEV_DO_MULTIPATH) == 0;
}

 
int ccw_device_is_pathgroup(struct ccw_device *cdev)
{
	return cdev->private->flags.pgroup;
}
EXPORT_SYMBOL(ccw_device_is_pathgroup);

 
int ccw_device_is_multipath(struct ccw_device *cdev)
{
	return cdev->private->flags.mpath;
}
EXPORT_SYMBOL(ccw_device_is_multipath);

 
int ccw_device_clear(struct ccw_device *cdev, unsigned long intparm)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE &&
	    cdev->private->state != DEV_STATE_W4SENSE)
		return -EINVAL;

	ret = cio_clear(sch);
	if (ret == 0)
		cdev->private->intparm = intparm;
	return ret;
}

 
int ccw_device_start_timeout_key(struct ccw_device *cdev, struct ccw1 *cpa,
				 unsigned long intparm, __u8 lpm, __u8 key,
				 unsigned long flags, int expires)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state == DEV_STATE_VERIFY) {
		 
		if (!cdev->private->flags.fake_irb) {
			cdev->private->flags.fake_irb = FAKE_CMD_IRB;
			cdev->private->intparm = intparm;
			return 0;
		} else
			 
			return -EBUSY;
	}
	if (cdev->private->state != DEV_STATE_ONLINE ||
	    ((sch->schib.scsw.cmd.stctl & SCSW_STCTL_PRIM_STATUS) &&
	     !(sch->schib.scsw.cmd.stctl & SCSW_STCTL_SEC_STATUS)) ||
	    cdev->private->flags.doverify)
		return -EBUSY;
	ret = cio_set_options (sch, flags);
	if (ret)
		return ret;
	 
	if (lpm) {
		lpm &= sch->lpm;
		if (lpm == 0)
			return -EACCES;
	}
	ret = cio_start_key (sch, cpa, lpm, key);
	switch (ret) {
	case 0:
		cdev->private->intparm = intparm;
		if (expires)
			ccw_device_set_timeout(cdev, expires);
		break;
	case -EACCES:
	case -ENODEV:
		dev_fsm_event(cdev, DEV_EVENT_VERIFY);
		break;
	}
	return ret;
}

 
int ccw_device_start_key(struct ccw_device *cdev, struct ccw1 *cpa,
			 unsigned long intparm, __u8 lpm, __u8 key,
			 unsigned long flags)
{
	return ccw_device_start_timeout_key(cdev, cpa, intparm, lpm, key,
					    flags, 0);
}

 
int ccw_device_start(struct ccw_device *cdev, struct ccw1 *cpa,
		     unsigned long intparm, __u8 lpm, unsigned long flags)
{
	return ccw_device_start_key(cdev, cpa, intparm, lpm,
				    PAGE_DEFAULT_KEY, flags);
}

 
int ccw_device_start_timeout(struct ccw_device *cdev, struct ccw1 *cpa,
			     unsigned long intparm, __u8 lpm,
			     unsigned long flags, int expires)
{
	return ccw_device_start_timeout_key(cdev, cpa, intparm, lpm,
					    PAGE_DEFAULT_KEY, flags,
					    expires);
}


 
int ccw_device_halt(struct ccw_device *cdev, unsigned long intparm)
{
	struct subchannel *sch;
	int ret;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE &&
	    cdev->private->state != DEV_STATE_W4SENSE)
		return -EINVAL;

	ret = cio_halt(sch);
	if (ret == 0)
		cdev->private->intparm = intparm;
	return ret;
}

 
int ccw_device_resume(struct ccw_device *cdev)
{
	struct subchannel *sch;

	if (!cdev || !cdev->dev.parent)
		return -ENODEV;
	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_NOT_OPER)
		return -ENODEV;
	if (cdev->private->state != DEV_STATE_ONLINE ||
	    !(sch->schib.scsw.cmd.actl & SCSW_ACTL_SUSPENDED))
		return -EINVAL;
	return cio_resume(sch);
}

 
struct ciw *ccw_device_get_ciw(struct ccw_device *cdev, __u32 ct)
{
	int ciw_cnt;

	if (cdev->private->flags.esid == 0)
		return NULL;
	for (ciw_cnt = 0; ciw_cnt < MAX_CIWS; ciw_cnt++)
		if (cdev->private->dma_area->senseid.ciw[ciw_cnt].ct == ct)
			return cdev->private->dma_area->senseid.ciw + ciw_cnt;
	return NULL;
}

 
__u8 ccw_device_get_path_mask(struct ccw_device *cdev)
{
	struct subchannel *sch;

	if (!cdev->dev.parent)
		return 0;

	sch = to_subchannel(cdev->dev.parent);
	return sch->lpm;
}

 
struct channel_path_desc_fmt0 *ccw_device_get_chp_desc(struct ccw_device *cdev,
						       int chp_idx)
{
	struct subchannel *sch;
	struct chp_id chpid;

	sch = to_subchannel(cdev->dev.parent);
	chp_id_init(&chpid);
	chpid.id = sch->schib.pmcw.chpid[chp_idx];
	return chp_get_chp_desc(chpid);
}

 
u8 *ccw_device_get_util_str(struct ccw_device *cdev, int chp_idx)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	struct channel_path *chp;
	struct chp_id chpid;
	u8 *util_str;

	chp_id_init(&chpid);
	chpid.id = sch->schib.pmcw.chpid[chp_idx];
	chp = chpid_to_chp(chpid);

	util_str = kmalloc(sizeof(chp->desc_fmt3.util_str), GFP_KERNEL);
	if (!util_str)
		return NULL;

	mutex_lock(&chp->lock);
	memcpy(util_str, chp->desc_fmt3.util_str, sizeof(chp->desc_fmt3.util_str));
	mutex_unlock(&chp->lock);

	return util_str;
}

 
void ccw_device_get_id(struct ccw_device *cdev, struct ccw_dev_id *dev_id)
{
	*dev_id = cdev->private->dev_id;
}
EXPORT_SYMBOL(ccw_device_get_id);

 
int ccw_device_tm_start_timeout_key(struct ccw_device *cdev, struct tcw *tcw,
				    unsigned long intparm, u8 lpm, u8 key,
				    int expires)
{
	struct subchannel *sch;
	int rc;

	sch = to_subchannel(cdev->dev.parent);
	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state == DEV_STATE_VERIFY) {
		 
		if (!cdev->private->flags.fake_irb) {
			cdev->private->flags.fake_irb = FAKE_TM_IRB;
			cdev->private->intparm = intparm;
			return 0;
		} else
			 
			return -EBUSY;
	}
	if (cdev->private->state != DEV_STATE_ONLINE)
		return -EIO;
	 
	if (lpm) {
		lpm &= sch->lpm;
		if (lpm == 0)
			return -EACCES;
	}
	rc = cio_tm_start_key(sch, tcw, lpm, key);
	if (rc == 0) {
		cdev->private->intparm = intparm;
		if (expires)
			ccw_device_set_timeout(cdev, expires);
	}
	return rc;
}
EXPORT_SYMBOL(ccw_device_tm_start_timeout_key);

 
int ccw_device_tm_start_key(struct ccw_device *cdev, struct tcw *tcw,
			    unsigned long intparm, u8 lpm, u8 key)
{
	return ccw_device_tm_start_timeout_key(cdev, tcw, intparm, lpm, key, 0);
}
EXPORT_SYMBOL(ccw_device_tm_start_key);

 
int ccw_device_tm_start(struct ccw_device *cdev, struct tcw *tcw,
			unsigned long intparm, u8 lpm)
{
	return ccw_device_tm_start_key(cdev, tcw, intparm, lpm,
				       PAGE_DEFAULT_KEY);
}
EXPORT_SYMBOL(ccw_device_tm_start);

 
int ccw_device_tm_start_timeout(struct ccw_device *cdev, struct tcw *tcw,
			       unsigned long intparm, u8 lpm, int expires)
{
	return ccw_device_tm_start_timeout_key(cdev, tcw, intparm, lpm,
					       PAGE_DEFAULT_KEY, expires);
}
EXPORT_SYMBOL(ccw_device_tm_start_timeout);

 
int ccw_device_get_mdc(struct ccw_device *cdev, u8 mask)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	struct channel_path *chp;
	struct chp_id chpid;
	int mdc = 0, i;

	 
	if (mask)
		mask &= sch->lpm;
	else
		mask = sch->lpm;

	chp_id_init(&chpid);
	for (i = 0; i < 8; i++) {
		if (!(mask & (0x80 >> i)))
			continue;
		chpid.id = sch->schib.pmcw.chpid[i];
		chp = chpid_to_chp(chpid);
		if (!chp)
			continue;

		mutex_lock(&chp->lock);
		if (!chp->desc_fmt1.f) {
			mutex_unlock(&chp->lock);
			return 0;
		}
		if (!chp->desc_fmt1.r)
			mdc = 1;
		mdc = mdc ? min_t(int, mdc, chp->desc_fmt1.mdc) :
			    chp->desc_fmt1.mdc;
		mutex_unlock(&chp->lock);
	}

	return mdc;
}
EXPORT_SYMBOL(ccw_device_get_mdc);

 
int ccw_device_tm_intrg(struct ccw_device *cdev)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	if (!sch->schib.pmcw.ena)
		return -EINVAL;
	if (cdev->private->state != DEV_STATE_ONLINE)
		return -EIO;
	if (!scsw_is_tm(&sch->schib.scsw) ||
	    !(scsw_actl(&sch->schib.scsw) & SCSW_ACTL_START_PEND))
		return -EINVAL;
	return cio_tm_intrg(sch);
}
EXPORT_SYMBOL(ccw_device_tm_intrg);

 
void ccw_device_get_schid(struct ccw_device *cdev, struct subchannel_id *schid)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);

	*schid = sch->schid;
}
EXPORT_SYMBOL_GPL(ccw_device_get_schid);

 
int ccw_device_pnso(struct ccw_device *cdev,
		    struct chsc_pnso_area *pnso_area, u8 oc,
		    struct chsc_pnso_resume_token resume_token, int cnc)
{
	struct subchannel_id schid;

	ccw_device_get_schid(cdev, &schid);
	return chsc_pnso(schid, pnso_area, oc, resume_token, cnc);
}
EXPORT_SYMBOL_GPL(ccw_device_pnso);

 
int ccw_device_get_cssid(struct ccw_device *cdev, u8 *cssid)
{
	struct device *sch_dev = cdev->dev.parent;
	struct channel_subsystem *css = to_css(sch_dev->parent);

	if (css->id_valid)
		*cssid = css->cssid;
	return css->id_valid ? 0 : -ENODEV;
}
EXPORT_SYMBOL_GPL(ccw_device_get_cssid);

 
int ccw_device_get_iid(struct ccw_device *cdev, u8 *iid)
{
	struct device *sch_dev = cdev->dev.parent;
	struct channel_subsystem *css = to_css(sch_dev->parent);

	if (css->id_valid)
		*iid = css->iid;
	return css->id_valid ? 0 : -ENODEV;
}
EXPORT_SYMBOL_GPL(ccw_device_get_iid);

 
int ccw_device_get_chpid(struct ccw_device *cdev, int chp_idx, u8 *chpid)
{
	struct subchannel *sch = to_subchannel(cdev->dev.parent);
	int mask;

	if ((chp_idx < 0) || (chp_idx > 7))
		return -EINVAL;
	mask = 0x80 >> chp_idx;
	if (!(sch->schib.pmcw.pim & mask))
		return -ENODEV;

	*chpid = sch->schib.pmcw.chpid[chp_idx];
	return 0;
}
EXPORT_SYMBOL_GPL(ccw_device_get_chpid);

 
int ccw_device_get_chid(struct ccw_device *cdev, int chp_idx, u16 *chid)
{
	struct chp_id cssid_chpid;
	struct channel_path *chp;
	int rc;

	chp_id_init(&cssid_chpid);
	rc = ccw_device_get_chpid(cdev, chp_idx, &cssid_chpid.id);
	if (rc)
		return rc;
	chp = chpid_to_chp(cssid_chpid);
	if (!chp)
		return -ENODEV;

	mutex_lock(&chp->lock);
	if (chp->desc_fmt1.flags & 0x10)
		*chid = chp->desc_fmt1.chid;
	else
		rc = -ENODEV;
	mutex_unlock(&chp->lock);

	return rc;
}
EXPORT_SYMBOL_GPL(ccw_device_get_chid);

 
void *ccw_device_dma_zalloc(struct ccw_device *cdev, size_t size)
{
	void *addr;

	if (!get_device(&cdev->dev))
		return NULL;
	addr = cio_gp_dma_zalloc(cdev->private->dma_pool, &cdev->dev, size);
	if (IS_ERR_OR_NULL(addr))
		put_device(&cdev->dev);
	return addr;
}
EXPORT_SYMBOL(ccw_device_dma_zalloc);

void ccw_device_dma_free(struct ccw_device *cdev, void *cpu_addr, size_t size)
{
	if (!cpu_addr)
		return;
	cio_gp_dma_free(cdev->private->dma_pool, cpu_addr, size);
	put_device(&cdev->dev);
}
EXPORT_SYMBOL(ccw_device_dma_free);

EXPORT_SYMBOL(ccw_device_set_options_mask);
EXPORT_SYMBOL(ccw_device_set_options);
EXPORT_SYMBOL(ccw_device_clear_options);
EXPORT_SYMBOL(ccw_device_clear);
EXPORT_SYMBOL(ccw_device_halt);
EXPORT_SYMBOL(ccw_device_resume);
EXPORT_SYMBOL(ccw_device_start_timeout);
EXPORT_SYMBOL(ccw_device_start);
EXPORT_SYMBOL(ccw_device_start_timeout_key);
EXPORT_SYMBOL(ccw_device_start_key);
EXPORT_SYMBOL(ccw_device_get_ciw);
EXPORT_SYMBOL(ccw_device_get_path_mask);
EXPORT_SYMBOL_GPL(ccw_device_get_chp_desc);
EXPORT_SYMBOL_GPL(ccw_device_get_util_str);
