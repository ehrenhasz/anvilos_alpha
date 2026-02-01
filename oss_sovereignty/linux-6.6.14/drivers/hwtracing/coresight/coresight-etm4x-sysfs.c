
 

#include <linux/pid_namespace.h>
#include <linux/pm_runtime.h>
#include <linux/sysfs.h>
#include "coresight-etm4x.h"
#include "coresight-priv.h"
#include "coresight-syscfg.h"

static int etm4_set_mode_exclude(struct etmv4_drvdata *drvdata, bool exclude)
{
	u8 idx;
	struct etmv4_config *config = &drvdata->config;

	idx = config->addr_idx;

	 
	if (FIELD_GET(TRCACATRn_TYPE_MASK, config->addr_acc[idx]) == TRCACATRn_TYPE_ADDR) {
		if (idx % 2 != 0)
			return -EINVAL;

		 
		if (config->addr_type[idx] != ETM_ADDR_TYPE_RANGE ||
		    config->addr_type[idx + 1] != ETM_ADDR_TYPE_RANGE)
			return -EINVAL;

		if (exclude == true) {
			 
			config->viiectlr |= BIT(idx / 2 + 16);
			config->viiectlr &= ~BIT(idx / 2);
		} else {
			 
			config->viiectlr |= BIT(idx / 2);
			config->viiectlr &= ~BIT(idx / 2 + 16);
		}
	}
	return 0;
}

static ssize_t nr_pe_cmp_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_pe_cmp;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_pe_cmp);

static ssize_t nr_addr_cmp_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_addr_cmp;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_addr_cmp);

static ssize_t nr_cntr_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_cntr;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_cntr);

static ssize_t nr_ext_inp_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_ext_inp;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_ext_inp);

static ssize_t numcidc_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->numcidc;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(numcidc);

static ssize_t numvmidc_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->numvmidc;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(numvmidc);

static ssize_t nrseqstate_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nrseqstate;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nrseqstate);

static ssize_t nr_resource_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_resource;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_resource);

static ssize_t nr_ss_cmp_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->nr_ss_cmp;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(nr_ss_cmp);

static ssize_t reset_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int i;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	if (val)
		config->mode = 0x0;

	 
	config->mode &= ~(ETM_MODE_LOAD | ETM_MODE_STORE);
	config->cfg &= ~(TRCCONFIGR_INSTP0_LOAD | TRCCONFIGR_INSTP0_STORE);

	 
	config->mode &= ~(ETM_MODE_DATA_TRACE_ADDR |
			   ETM_MODE_DATA_TRACE_VAL);
	config->cfg &= ~(TRCCONFIGR_DA | TRCCONFIGR_DV);

	 
	config->eventctrl0 = 0x0;
	config->eventctrl1 = 0x0;

	 
	config->ts_ctrl = 0x0;

	 
	config->stall_ctrl = 0x0;

	 
	if (drvdata->syncpr == false)
		config->syncfreq = 0x8;

	 
	config->vinst_ctrl = FIELD_PREP(TRCVICTLR_EVENT_MASK, 0x01);
	if (drvdata->nr_addr_cmp > 0) {
		config->mode |= ETM_MODE_VIEWINST_STARTSTOP;
		 
		config->vinst_ctrl |= TRCVICTLR_SSSTATUS;
	}

	 
	config->viiectlr = 0x0;

	 
	config->vissctlr = 0x0;
	config->vipcssctlr = 0x0;

	 
	for (i = 0; i < drvdata->nrseqstate-1; i++)
		config->seq_ctrl[i] = 0x0;
	config->seq_rst = 0x0;
	config->seq_state = 0x0;

	 
	config->ext_inp = 0x0;

	config->cntr_idx = 0x0;
	for (i = 0; i < drvdata->nr_cntr; i++) {
		config->cntrldvr[i] = 0x0;
		config->cntr_ctrl[i] = 0x0;
		config->cntr_val[i] = 0x0;
	}

	config->res_idx = 0x0;
	for (i = 2; i < 2 * drvdata->nr_resource; i++)
		config->res_ctrl[i] = 0x0;

	config->ss_idx = 0x0;
	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		config->ss_ctrl[i] = 0x0;
		config->ss_pe_cmp[i] = 0x0;
	}

	config->addr_idx = 0x0;
	for (i = 0; i < drvdata->nr_addr_cmp * 2; i++) {
		config->addr_val[i] = 0x0;
		config->addr_acc[i] = 0x0;
		config->addr_type[i] = ETM_ADDR_TYPE_NONE;
	}

	config->ctxid_idx = 0x0;
	for (i = 0; i < drvdata->numcidc; i++)
		config->ctxid_pid[i] = 0x0;

	config->ctxid_mask0 = 0x0;
	config->ctxid_mask1 = 0x0;

	config->vmid_idx = 0x0;
	for (i = 0; i < drvdata->numvmidc; i++)
		config->vmid_val[i] = 0x0;
	config->vmid_mask0 = 0x0;
	config->vmid_mask1 = 0x0;

	spin_unlock(&drvdata->spinlock);

	 
	etm4_release_trace_id(drvdata);

	cscfg_csdev_reset_feats(to_coresight_device(dev));

	return size;
}
static DEVICE_ATTR_WO(reset);

static ssize_t mode_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->mode;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t mode_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t size)
{
	unsigned long val, mode;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->mode = val & ETMv4_MODE_ALL;

	if (drvdata->instrp0 == true) {
		 
		config->cfg  &= ~TRCCONFIGR_INSTP0_LOAD_STORE;
		if (config->mode & ETM_MODE_LOAD)
			 
			config->cfg  |= TRCCONFIGR_INSTP0_LOAD;
		if (config->mode & ETM_MODE_STORE)
			 
			config->cfg  |= TRCCONFIGR_INSTP0_STORE;
		if (config->mode & ETM_MODE_LOAD_STORE)
			 
			config->cfg  |= TRCCONFIGR_INSTP0_LOAD_STORE;
	}

	 
	if ((config->mode & ETM_MODE_BB) && (drvdata->trcbb == true))
		config->cfg |= TRCCONFIGR_BB;
	else
		config->cfg &= ~TRCCONFIGR_BB;

	 
	if ((config->mode & ETMv4_MODE_CYCACC) &&
		(drvdata->trccci == true))
		config->cfg |= TRCCONFIGR_CCI;
	else
		config->cfg &= ~TRCCONFIGR_CCI;

	 
	if ((config->mode & ETMv4_MODE_CTXID) && (drvdata->ctxid_size))
		config->cfg |= TRCCONFIGR_CID;
	else
		config->cfg &= ~TRCCONFIGR_CID;

	if ((config->mode & ETM_MODE_VMID) && (drvdata->vmid_size))
		config->cfg |= TRCCONFIGR_VMID;
	else
		config->cfg &= ~TRCCONFIGR_VMID;

	 
	mode = ETM_MODE_COND(config->mode);
	if (drvdata->trccond == true) {
		config->cfg &= ~TRCCONFIGR_COND_MASK;
		config->cfg |= mode << __bf_shf(TRCCONFIGR_COND_MASK);
	}

	 
	if ((config->mode & ETMv4_MODE_TIMESTAMP) && (drvdata->ts_size))
		config->cfg |= TRCCONFIGR_TS;
	else
		config->cfg &= ~TRCCONFIGR_TS;

	 
	if ((config->mode & ETM_MODE_RETURNSTACK) &&
					(drvdata->retstack == true))
		config->cfg |= TRCCONFIGR_RS;
	else
		config->cfg &= ~TRCCONFIGR_RS;

	 
	mode = ETM_MODE_QELEM(config->mode);
	 
	config->cfg &= ~(TRCCONFIGR_QE_W_COUNTS | TRCCONFIGR_QE_WO_COUNTS);
	 
	if (mode && drvdata->q_support)
		config->cfg |= TRCCONFIGR_QE_W_COUNTS;
	 
	if ((mode & BIT(1)) && (drvdata->q_support & BIT(1)))
		config->cfg |= TRCCONFIGR_QE_WO_COUNTS;

	 
	if ((config->mode & ETM_MODE_ATB_TRIGGER) &&
	    (drvdata->atbtrig == true))
		config->eventctrl1 |= TRCEVENTCTL1R_ATB;
	else
		config->eventctrl1 &= ~TRCEVENTCTL1R_ATB;

	 
	if ((config->mode & ETM_MODE_LPOVERRIDE) &&
	    (drvdata->lpoverride == true))
		config->eventctrl1 |= TRCEVENTCTL1R_LPOVERRIDE;
	else
		config->eventctrl1 &= ~TRCEVENTCTL1R_LPOVERRIDE;

	 
	if ((config->mode & ETM_MODE_ISTALL_EN) && (drvdata->stallctl == true))
		config->stall_ctrl |= TRCSTALLCTLR_ISTALL;
	else
		config->stall_ctrl &= ~TRCSTALLCTLR_ISTALL;

	 
	if (config->mode & ETM_MODE_INSTPRIO)
		config->stall_ctrl |= TRCSTALLCTLR_INSTPRIORITY;
	else
		config->stall_ctrl &= ~TRCSTALLCTLR_INSTPRIORITY;

	 
	if ((config->mode & ETM_MODE_NOOVERFLOW) &&
		(drvdata->nooverflow == true))
		config->stall_ctrl |= TRCSTALLCTLR_NOOVERFLOW;
	else
		config->stall_ctrl &= ~TRCSTALLCTLR_NOOVERFLOW;

	 
	if (config->mode & ETM_MODE_VIEWINST_STARTSTOP)
		config->vinst_ctrl |= TRCVICTLR_SSSTATUS;
	else
		config->vinst_ctrl &= ~TRCVICTLR_SSSTATUS;

	 
	if (config->mode & ETM_MODE_TRACE_RESET)
		config->vinst_ctrl |= TRCVICTLR_TRCRESET;
	else
		config->vinst_ctrl &= ~TRCVICTLR_TRCRESET;

	 
	if ((config->mode & ETM_MODE_TRACE_ERR) &&
		(drvdata->trc_error == true))
		config->vinst_ctrl |= TRCVICTLR_TRCERR;
	else
		config->vinst_ctrl &= ~TRCVICTLR_TRCERR;

	if (config->mode & (ETM_MODE_EXCL_KERN | ETM_MODE_EXCL_USER))
		etm4_config_trace_mode(config);

	spin_unlock(&drvdata->spinlock);

	return size;
}
static DEVICE_ATTR_RW(mode);

static ssize_t pe_show(struct device *dev,
		       struct device_attribute *attr,
		       char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->pe_sel;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t pe_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	if (val > drvdata->nr_pe) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}

	config->pe_sel = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(pe);

static ssize_t event_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->eventctrl0;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t event_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	switch (drvdata->nr_event) {
	case 0x0:
		 
		config->eventctrl0 = val & 0xFF;
		break;
	case 0x1:
		  
		config->eventctrl0 = val & 0xFFFF;
		break;
	case 0x2:
		 
		config->eventctrl0 = val & 0xFFFFFF;
		break;
	case 0x3:
		 
		config->eventctrl0 = val;
		break;
	default:
		break;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(event);

static ssize_t event_instren_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = FIELD_GET(TRCEVENTCTL1R_INSTEN_MASK, config->eventctrl1);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t event_instren_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	 
	config->eventctrl1 &= ~TRCEVENTCTL1R_INSTEN_MASK;
	switch (drvdata->nr_event) {
	case 0x0:
		 
		config->eventctrl1 |= val & TRCEVENTCTL1R_INSTEN_1;
		break;
	case 0x1:
		 
		config->eventctrl1 |= val & (TRCEVENTCTL1R_INSTEN_0 | TRCEVENTCTL1R_INSTEN_1);
		break;
	case 0x2:
		 
		config->eventctrl1 |= val & (TRCEVENTCTL1R_INSTEN_0 |
					     TRCEVENTCTL1R_INSTEN_1 |
					     TRCEVENTCTL1R_INSTEN_2);
		break;
	case 0x3:
		 
		config->eventctrl1 |= val & (TRCEVENTCTL1R_INSTEN_0 |
					     TRCEVENTCTL1R_INSTEN_1 |
					     TRCEVENTCTL1R_INSTEN_2 |
					     TRCEVENTCTL1R_INSTEN_3);
		break;
	default:
		break;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(event_instren);

static ssize_t event_ts_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->ts_ctrl;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t event_ts_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!drvdata->ts_size)
		return -EINVAL;

	config->ts_ctrl = val & ETMv4_EVENT_MASK;
	return size;
}
static DEVICE_ATTR_RW(event_ts);

static ssize_t syncfreq_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->syncfreq;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t syncfreq_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (drvdata->syncpr == true)
		return -EINVAL;

	config->syncfreq = val & ETMv4_SYNC_MASK;
	return size;
}
static DEVICE_ATTR_RW(syncfreq);

static ssize_t cyc_threshold_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->ccctlr;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cyc_threshold_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	 
	val &= ETM_CYC_THRESHOLD_MASK;
	if (val < drvdata->ccitmin)
		return -EINVAL;

	config->ccctlr = val;
	return size;
}
static DEVICE_ATTR_RW(cyc_threshold);

static ssize_t bb_ctrl_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->bb_ctrl;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t bb_ctrl_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (drvdata->trcbb == false)
		return -EINVAL;
	if (!drvdata->nr_addr_cmp)
		return -EINVAL;

	 
	if ((val & TRCBBCTLR_MODE) && (FIELD_GET(TRCBBCTLR_RANGE_MASK, val) == 0))
		return -EINVAL;

	config->bb_ctrl = val & (TRCBBCTLR_MODE | TRCBBCTLR_RANGE_MASK);
	return size;
}
static DEVICE_ATTR_RW(bb_ctrl);

static ssize_t event_vinst_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = FIELD_GET(TRCVICTLR_EVENT_MASK, config->vinst_ctrl);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t event_vinst_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	val &= TRCVICTLR_EVENT_MASK >> __bf_shf(TRCVICTLR_EVENT_MASK);
	config->vinst_ctrl &= ~TRCVICTLR_EVENT_MASK;
	config->vinst_ctrl |= FIELD_PREP(TRCVICTLR_EVENT_MASK, val);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(event_vinst);

static ssize_t s_exlevel_vinst_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = FIELD_GET(TRCVICTLR_EXLEVEL_S_MASK, config->vinst_ctrl);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t s_exlevel_vinst_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	 
	config->vinst_ctrl &= ~TRCVICTLR_EXLEVEL_S_MASK;
	 
	val &= drvdata->s_ex_level;
	config->vinst_ctrl |= val << __bf_shf(TRCVICTLR_EXLEVEL_S_MASK);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(s_exlevel_vinst);

static ssize_t ns_exlevel_vinst_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	val = FIELD_GET(TRCVICTLR_EXLEVEL_NS_MASK, config->vinst_ctrl);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ns_exlevel_vinst_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	 
	config->vinst_ctrl &= ~TRCVICTLR_EXLEVEL_NS_MASK;
	 
	val &= drvdata->ns_ex_level;
	config->vinst_ctrl |= val << __bf_shf(TRCVICTLR_EXLEVEL_NS_MASK);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(ns_exlevel_vinst);

static ssize_t addr_idx_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->addr_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_idx_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->nr_addr_cmp * 2)
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->addr_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_idx);

static ssize_t addr_instdatatype_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	ssize_t len;
	u8 val, idx;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	val = FIELD_GET(TRCACATRn_TYPE_MASK, config->addr_acc[idx]);
	len = scnprintf(buf, PAGE_SIZE, "%s\n",
			val == TRCACATRn_TYPE_ADDR ? "instr" :
			(val == TRCACATRn_TYPE_DATA_LOAD_ADDR ? "data_load" :
			(val == TRCACATRn_TYPE_DATA_STORE_ADDR ? "data_store" :
			"data_load_store")));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t addr_instdatatype_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	u8 idx;
	char str[20] = "";
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (strlen(buf) >= 20)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (!strcmp(str, "instr"))
		 
		config->addr_acc[idx] &= ~TRCACATRn_TYPE_MASK;

	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_instdatatype);

static ssize_t addr_single_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	idx = config->addr_idx;
	spin_lock(&drvdata->spinlock);
	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      config->addr_type[idx] == ETM_ADDR_TYPE_SINGLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	val = (unsigned long)config->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_single_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      config->addr_type[idx] == ETM_ADDR_TYPE_SINGLE)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	config->addr_val[idx] = (u64)val;
	config->addr_type[idx] = ETM_ADDR_TYPE_SINGLE;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_single);

static ssize_t addr_range_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	u8 idx;
	unsigned long val1, val2;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}
	if (!((config->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       config->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (config->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       config->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val1 = (unsigned long)config->addr_val[idx];
	val2 = (unsigned long)config->addr_val[idx + 1];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t addr_range_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 idx;
	unsigned long val1, val2;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;
	int elements, exclude;

	elements = sscanf(buf, "%lx %lx %x", &val1, &val2, &exclude);

	 
	if (elements < 2)
		return -EINVAL;
	 
	if (val1 > val2)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (idx % 2 != 0) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	if (!((config->addr_type[idx] == ETM_ADDR_TYPE_NONE &&
	       config->addr_type[idx + 1] == ETM_ADDR_TYPE_NONE) ||
	      (config->addr_type[idx] == ETM_ADDR_TYPE_RANGE &&
	       config->addr_type[idx + 1] == ETM_ADDR_TYPE_RANGE))) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	config->addr_val[idx] = (u64)val1;
	config->addr_type[idx] = ETM_ADDR_TYPE_RANGE;
	config->addr_val[idx + 1] = (u64)val2;
	config->addr_type[idx + 1] = ETM_ADDR_TYPE_RANGE;
	 
	if (elements != 3)
		exclude = config->mode & ETM_MODE_EXCLUDE;
	etm4_set_mode_exclude(drvdata, exclude ? true : false);

	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_range);

static ssize_t addr_start_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;

	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      config->addr_type[idx] == ETM_ADDR_TYPE_START)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = (unsigned long)config->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_start_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (!drvdata->nr_addr_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      config->addr_type[idx] == ETM_ADDR_TYPE_START)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	config->addr_val[idx] = (u64)val;
	config->addr_type[idx] = ETM_ADDR_TYPE_START;
	config->vissctlr |= BIT(idx);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_start);

static ssize_t addr_stop_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;

	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	      config->addr_type[idx] == ETM_ADDR_TYPE_STOP)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	val = (unsigned long)config->addr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_stop_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (!drvdata->nr_addr_cmp) {
		spin_unlock(&drvdata->spinlock);
		return -EINVAL;
	}
	if (!(config->addr_type[idx] == ETM_ADDR_TYPE_NONE ||
	       config->addr_type[idx] == ETM_ADDR_TYPE_STOP)) {
		spin_unlock(&drvdata->spinlock);
		return -EPERM;
	}

	config->addr_val[idx] = (u64)val;
	config->addr_type[idx] = ETM_ADDR_TYPE_STOP;
	config->vissctlr |= BIT(idx + 16);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_stop);

static ssize_t addr_ctxtype_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	ssize_t len;
	u8 idx, val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	 
	val = FIELD_GET(TRCACATRn_CONTEXTTYPE_MASK, config->addr_acc[idx]);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", val == ETM_CTX_NONE ? "none" :
			(val == ETM_CTX_CTXID ? "ctxid" :
			(val == ETM_CTX_VMID ? "vmid" : "all")));
	spin_unlock(&drvdata->spinlock);
	return len;
}

static ssize_t addr_ctxtype_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	u8 idx;
	char str[10] = "";
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (strlen(buf) >= 10)
		return -EINVAL;
	if (sscanf(buf, "%s", str) != 1)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	if (!strcmp(str, "none"))
		 
		config->addr_acc[idx] &= ~TRCACATRn_CONTEXTTYPE_MASK;
	else if (!strcmp(str, "ctxid")) {
		 
		if (drvdata->numcidc) {
			config->addr_acc[idx] |= TRCACATRn_CONTEXTTYPE_CTXID;
			config->addr_acc[idx] &= ~TRCACATRn_CONTEXTTYPE_VMID;
		}
	} else if (!strcmp(str, "vmid")) {
		 
		if (drvdata->numvmidc) {
			config->addr_acc[idx] &= ~TRCACATRn_CONTEXTTYPE_CTXID;
			config->addr_acc[idx] |= TRCACATRn_CONTEXTTYPE_VMID;
		}
	} else if (!strcmp(str, "all")) {
		 
		if (drvdata->numcidc)
			config->addr_acc[idx] |= TRCACATRn_CONTEXTTYPE_CTXID;
		if (drvdata->numvmidc)
			config->addr_acc[idx] |= TRCACATRn_CONTEXTTYPE_VMID;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_ctxtype);

static ssize_t addr_context_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	 
	val = FIELD_GET(TRCACATRn_CONTEXT_MASK, config->addr_acc[idx]);
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_context_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if ((drvdata->numcidc <= 1) && (drvdata->numvmidc <= 1))
		return -EINVAL;
	if (val >=  (drvdata->numcidc >= drvdata->numvmidc ?
		     drvdata->numcidc : drvdata->numvmidc))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	 
	config->addr_acc[idx] &= ~TRCACATRn_CONTEXT_MASK;
	config->addr_acc[idx] |= val << __bf_shf(TRCACATRn_CONTEXT_MASK);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_context);

static ssize_t addr_exlevel_s_ns_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	val = FIELD_GET(TRCACATRn_EXLEVEL_MASK, config->addr_acc[idx]);
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t addr_exlevel_s_ns_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val & ~(TRCACATRn_EXLEVEL_MASK >> __bf_shf(TRCACATRn_EXLEVEL_MASK)))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	 
	config->addr_acc[idx] &= ~TRCACATRn_EXLEVEL_MASK;
	config->addr_acc[idx] |= val << __bf_shf(TRCACATRn_EXLEVEL_MASK);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(addr_exlevel_s_ns);

static const char * const addr_type_names[] = {
	"unused",
	"single",
	"range",
	"start",
	"stop"
};

static ssize_t addr_cmp_view_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	u8 idx, addr_type;
	unsigned long addr_v, addr_v2, addr_ctrl;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;
	int size = 0;
	bool exclude = false;

	spin_lock(&drvdata->spinlock);
	idx = config->addr_idx;
	addr_v = config->addr_val[idx];
	addr_ctrl = config->addr_acc[idx];
	addr_type = config->addr_type[idx];
	if (addr_type == ETM_ADDR_TYPE_RANGE) {
		if (idx & 0x1) {
			idx -= 1;
			addr_v2 = addr_v;
			addr_v = config->addr_val[idx];
		} else {
			addr_v2 = config->addr_val[idx + 1];
		}
		exclude = config->viiectlr & BIT(idx / 2 + 16);
	}
	spin_unlock(&drvdata->spinlock);
	if (addr_type) {
		size = scnprintf(buf, PAGE_SIZE, "addr_cmp[%i] %s %#lx", idx,
				 addr_type_names[addr_type], addr_v);
		if (addr_type == ETM_ADDR_TYPE_RANGE) {
			size += scnprintf(buf + size, PAGE_SIZE - size,
					  " %#lx %s", addr_v2,
					  exclude ? "exclude" : "include");
		}
		size += scnprintf(buf + size, PAGE_SIZE - size,
				  " ctrl(%#lx)\n", addr_ctrl);
	} else {
		size = scnprintf(buf, PAGE_SIZE, "addr_cmp[%i] unused\n", idx);
	}
	return size;
}
static DEVICE_ATTR_RO(addr_cmp_view);

static ssize_t vinst_pe_cmp_start_stop_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (!drvdata->nr_pe_cmp)
		return -EINVAL;
	val = config->vipcssctlr;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static ssize_t vinst_pe_cmp_start_stop_store(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!drvdata->nr_pe_cmp)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->vipcssctlr = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(vinst_pe_cmp_start_stop);

static ssize_t seq_idx_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->seq_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t seq_idx_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->nrseqstate - 1)
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->seq_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(seq_idx);

static ssize_t seq_state_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->seq_state;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t seq_state_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->nrseqstate)
		return -EINVAL;

	config->seq_state = val;
	return size;
}
static DEVICE_ATTR_RW(seq_state);

static ssize_t seq_event_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->seq_idx;
	val = config->seq_ctrl[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t seq_event_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->seq_idx;
	 
	config->seq_ctrl[idx] = val & 0xFFFF;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(seq_event);

static ssize_t seq_reset_event_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->seq_rst;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t seq_reset_event_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (!(drvdata->nrseqstate))
		return -EINVAL;

	config->seq_rst = val & ETMv4_EVENT_MASK;
	return size;
}
static DEVICE_ATTR_RW(seq_reset_event);

static ssize_t cntr_idx_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->cntr_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cntr_idx_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->nr_cntr)
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->cntr_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(cntr_idx);

static ssize_t cntrldvr_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	val = config->cntrldvr[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cntrldvr_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val > ETM_CNTR_MAX_VAL)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	config->cntrldvr[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(cntrldvr);

static ssize_t cntr_val_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	val = config->cntr_val[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cntr_val_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val > ETM_CNTR_MAX_VAL)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	config->cntr_val[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(cntr_val);

static ssize_t cntr_ctrl_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	val = config->cntr_ctrl[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t cntr_ctrl_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->cntr_idx;
	config->cntr_ctrl[idx] = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(cntr_ctrl);

static ssize_t res_idx_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->res_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t res_idx_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	 
	if ((val < 2) || (val >= 2 * drvdata->nr_resource))
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->res_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(res_idx);

static ssize_t res_ctrl_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	idx = config->res_idx;
	val = config->res_ctrl[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t res_ctrl_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->res_idx;
	 
	if (idx % 2 != 0)
		 
		val &= ~TRCRSCTLRn_PAIRINV;
	config->res_ctrl[idx] = val & (TRCRSCTLRn_PAIRINV |
				       TRCRSCTLRn_INV |
				       TRCRSCTLRn_GROUP_MASK |
				       TRCRSCTLRn_SELECT_MASK);
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(res_ctrl);

static ssize_t sshot_idx_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->ss_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t sshot_idx_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->nr_ss_cmp)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->ss_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(sshot_idx);

static ssize_t sshot_ctrl_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	val = config->ss_ctrl[config->ss_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t sshot_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->ss_idx;
	config->ss_ctrl[idx] = FIELD_PREP(TRCSSCCRn_SAC_ARC_RST_MASK, val);
	 
	config->ss_status[idx] &= ~TRCSSCSRn_STATUS;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(sshot_ctrl);

static ssize_t sshot_status_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	val = config->ss_status[config->ss_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
static DEVICE_ATTR_RO(sshot_status);

static ssize_t sshot_pe_ctrl_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	spin_lock(&drvdata->spinlock);
	val = config->ss_pe_cmp[config->ss_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t sshot_pe_ctrl_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->ss_idx;
	config->ss_pe_cmp[idx] = FIELD_PREP(TRCSSPCICRn_PC_MASK, val);
	 
	config->ss_status[idx] &= ~TRCSSCSRn_STATUS;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(sshot_pe_ctrl);

static ssize_t ctxid_idx_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->ctxid_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ctxid_idx_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->numcidc)
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->ctxid_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(ctxid_idx);

static ssize_t ctxid_pid_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	u8 idx;
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (task_active_pid_ns(current) != &init_pid_ns)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->ctxid_idx;
	val = (unsigned long)config->ctxid_pid[idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t ctxid_pid_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t size)
{
	u8 idx;
	unsigned long pid;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (task_active_pid_ns(current) != &init_pid_ns)
		return -EINVAL;

	 
	if (!drvdata->ctxid_size || !drvdata->numcidc)
		return -EINVAL;
	if (kstrtoul(buf, 16, &pid))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	idx = config->ctxid_idx;
	config->ctxid_pid[idx] = (u64)pid;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(ctxid_pid);

static ssize_t ctxid_masks_show(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned long val1, val2;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (task_active_pid_ns(current) != &init_pid_ns)
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	val1 = config->ctxid_mask0;
	val2 = config->ctxid_mask1;
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t ctxid_masks_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 i, j, maskbyte;
	unsigned long val1, val2, mask;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;
	int nr_inputs;

	 
	if (task_active_pid_ns(current) != &init_pid_ns)
		return -EINVAL;

	 
	if (!drvdata->ctxid_size || !drvdata->numcidc)
		return -EINVAL;
	 
	nr_inputs = sscanf(buf, "%lx %lx", &val1, &val2);
	if ((drvdata->numcidc > 4) && (nr_inputs != 2))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	 
	switch (drvdata->numcidc) {
	case 0x1:
		 
		config->ctxid_mask0 = val1 & 0xFF;
		break;
	case 0x2:
		 
		config->ctxid_mask0 = val1 & 0xFFFF;
		break;
	case 0x3:
		 
		config->ctxid_mask0 = val1 & 0xFFFFFF;
		break;
	case 0x4:
		  
		config->ctxid_mask0 = val1;
		break;
	case 0x5:
		 
		config->ctxid_mask0 = val1;
		config->ctxid_mask1 = val2 & 0xFF;
		break;
	case 0x6:
		 
		config->ctxid_mask0 = val1;
		config->ctxid_mask1 = val2 & 0xFFFF;
		break;
	case 0x7:
		 
		config->ctxid_mask0 = val1;
		config->ctxid_mask1 = val2 & 0xFFFFFF;
		break;
	case 0x8:
		 
		config->ctxid_mask0 = val1;
		config->ctxid_mask1 = val2;
		break;
	default:
		break;
	}
	 
	mask = config->ctxid_mask0;
	for (i = 0; i < drvdata->numcidc; i++) {
		 
		maskbyte = mask & ETMv4_EVENT_MASK;
		 
		for (j = 0; j < 8; j++) {
			if (maskbyte & 1)
				config->ctxid_pid[i] &= ~(0xFFUL << (j * 8));
			maskbyte >>= 1;
		}
		 
		if (i == 3)
			 
			mask = config->ctxid_mask1;
		else
			mask >>= 0x8;
	}

	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(ctxid_masks);

static ssize_t vmid_idx_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	val = config->vmid_idx;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t vmid_idx_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	if (kstrtoul(buf, 16, &val))
		return -EINVAL;
	if (val >= drvdata->numvmidc)
		return -EINVAL;

	 
	spin_lock(&drvdata->spinlock);
	config->vmid_idx = val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(vmid_idx);

static ssize_t vmid_val_show(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (!task_is_in_init_pid_ns(current))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	val = (unsigned long)config->vmid_val[config->vmid_idx];
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}

static ssize_t vmid_val_store(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	unsigned long val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (!task_is_in_init_pid_ns(current))
		return -EINVAL;

	 
	if (!drvdata->vmid_size || !drvdata->numvmidc)
		return -EINVAL;
	if (kstrtoul(buf, 16, &val))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	config->vmid_val[config->vmid_idx] = (u64)val;
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(vmid_val);

static ssize_t vmid_masks_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	unsigned long val1, val2;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;

	 
	if (!task_is_in_init_pid_ns(current))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);
	val1 = config->vmid_mask0;
	val2 = config->vmid_mask1;
	spin_unlock(&drvdata->spinlock);
	return scnprintf(buf, PAGE_SIZE, "%#lx %#lx\n", val1, val2);
}

static ssize_t vmid_masks_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	u8 i, j, maskbyte;
	unsigned long val1, val2, mask;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct etmv4_config *config = &drvdata->config;
	int nr_inputs;

	 
	if (!task_is_in_init_pid_ns(current))
		return -EINVAL;

	 
	if (!drvdata->vmid_size || !drvdata->numvmidc)
		return -EINVAL;
	 
	nr_inputs = sscanf(buf, "%lx %lx", &val1, &val2);
	if ((drvdata->numvmidc > 4) && (nr_inputs != 2))
		return -EINVAL;

	spin_lock(&drvdata->spinlock);

	 
	switch (drvdata->numvmidc) {
	case 0x1:
		 
		config->vmid_mask0 = val1 & 0xFF;
		break;
	case 0x2:
		 
		config->vmid_mask0 = val1 & 0xFFFF;
		break;
	case 0x3:
		 
		config->vmid_mask0 = val1 & 0xFFFFFF;
		break;
	case 0x4:
		 
		config->vmid_mask0 = val1;
		break;
	case 0x5:
		 
		config->vmid_mask0 = val1;
		config->vmid_mask1 = val2 & 0xFF;
		break;
	case 0x6:
		 
		config->vmid_mask0 = val1;
		config->vmid_mask1 = val2 & 0xFFFF;
		break;
	case 0x7:
		 
		config->vmid_mask0 = val1;
		config->vmid_mask1 = val2 & 0xFFFFFF;
		break;
	case 0x8:
		 
		config->vmid_mask0 = val1;
		config->vmid_mask1 = val2;
		break;
	default:
		break;
	}

	 
	mask = config->vmid_mask0;
	for (i = 0; i < drvdata->numvmidc; i++) {
		 
		maskbyte = mask & ETMv4_EVENT_MASK;
		 
		for (j = 0; j < 8; j++) {
			if (maskbyte & 1)
				config->vmid_val[i] &= ~(0xFFUL << (j * 8));
			maskbyte >>= 1;
		}
		 
		if (i == 3)
			 
			mask = config->vmid_mask1;
		else
			mask >>= 0x8;
	}
	spin_unlock(&drvdata->spinlock);
	return size;
}
static DEVICE_ATTR_RW(vmid_masks);

static ssize_t cpu_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	val = drvdata->cpu;
	return scnprintf(buf, PAGE_SIZE, "%d\n", val);

}
static DEVICE_ATTR_RO(cpu);

static ssize_t ts_source_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	int val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	if (!drvdata->trfcr) {
		val = -1;
		goto out;
	}

	switch (drvdata->trfcr & TRFCR_ELx_TS_MASK) {
	case TRFCR_ELx_TS_VIRTUAL:
	case TRFCR_ELx_TS_GUEST_PHYSICAL:
	case TRFCR_ELx_TS_PHYSICAL:
		val = FIELD_GET(TRFCR_ELx_TS_MASK, drvdata->trfcr);
		break;
	default:
		val = -1;
		break;
	}

out:
	return sysfs_emit(buf, "%d\n", val);
}
static DEVICE_ATTR_RO(ts_source);

static struct attribute *coresight_etmv4_attrs[] = {
	&dev_attr_nr_pe_cmp.attr,
	&dev_attr_nr_addr_cmp.attr,
	&dev_attr_nr_cntr.attr,
	&dev_attr_nr_ext_inp.attr,
	&dev_attr_numcidc.attr,
	&dev_attr_numvmidc.attr,
	&dev_attr_nrseqstate.attr,
	&dev_attr_nr_resource.attr,
	&dev_attr_nr_ss_cmp.attr,
	&dev_attr_reset.attr,
	&dev_attr_mode.attr,
	&dev_attr_pe.attr,
	&dev_attr_event.attr,
	&dev_attr_event_instren.attr,
	&dev_attr_event_ts.attr,
	&dev_attr_syncfreq.attr,
	&dev_attr_cyc_threshold.attr,
	&dev_attr_bb_ctrl.attr,
	&dev_attr_event_vinst.attr,
	&dev_attr_s_exlevel_vinst.attr,
	&dev_attr_ns_exlevel_vinst.attr,
	&dev_attr_addr_idx.attr,
	&dev_attr_addr_instdatatype.attr,
	&dev_attr_addr_single.attr,
	&dev_attr_addr_range.attr,
	&dev_attr_addr_start.attr,
	&dev_attr_addr_stop.attr,
	&dev_attr_addr_ctxtype.attr,
	&dev_attr_addr_context.attr,
	&dev_attr_addr_exlevel_s_ns.attr,
	&dev_attr_addr_cmp_view.attr,
	&dev_attr_vinst_pe_cmp_start_stop.attr,
	&dev_attr_sshot_idx.attr,
	&dev_attr_sshot_ctrl.attr,
	&dev_attr_sshot_pe_ctrl.attr,
	&dev_attr_sshot_status.attr,
	&dev_attr_seq_idx.attr,
	&dev_attr_seq_state.attr,
	&dev_attr_seq_event.attr,
	&dev_attr_seq_reset_event.attr,
	&dev_attr_cntr_idx.attr,
	&dev_attr_cntrldvr.attr,
	&dev_attr_cntr_val.attr,
	&dev_attr_cntr_ctrl.attr,
	&dev_attr_res_idx.attr,
	&dev_attr_res_ctrl.attr,
	&dev_attr_ctxid_idx.attr,
	&dev_attr_ctxid_pid.attr,
	&dev_attr_ctxid_masks.attr,
	&dev_attr_vmid_idx.attr,
	&dev_attr_vmid_val.attr,
	&dev_attr_vmid_masks.attr,
	&dev_attr_cpu.attr,
	&dev_attr_ts_source.attr,
	NULL,
};

 
static ssize_t trctraceid_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	int trace_id;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	trace_id = etm4_read_alloc_trace_id(drvdata);
	if (trace_id < 0)
		return trace_id;

	return sysfs_emit(buf, "0x%x\n", trace_id);
}

struct etmv4_reg {
	struct coresight_device *csdev;
	u32 offset;
	u32 data;
};

static void do_smp_cross_read(void *data)
{
	struct etmv4_reg *reg = data;

	reg->data = etm4x_relaxed_read32(&reg->csdev->access, reg->offset);
}

static u32 etmv4_cross_read(const struct etmv4_drvdata *drvdata, u32 offset)
{
	struct etmv4_reg reg;

	reg.offset = offset;
	reg.csdev = drvdata->csdev;

	 
	smp_call_function_single(drvdata->cpu, do_smp_cross_read, &reg, 1);
	return reg.data;
}

static inline u32 coresight_etm4x_attr_to_offset(struct device_attribute *attr)
{
	struct dev_ext_attribute *eattr;

	eattr = container_of(attr, struct dev_ext_attribute, attr);
	return (u32)(unsigned long)eattr->var;
}

static ssize_t coresight_etm4x_reg_show(struct device *dev,
					struct device_attribute *d_attr,
					char *buf)
{
	u32 val, offset;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);

	offset = coresight_etm4x_attr_to_offset(d_attr);

	pm_runtime_get_sync(dev->parent);
	val = etmv4_cross_read(drvdata, offset);
	pm_runtime_put_sync(dev->parent);

	return scnprintf(buf, PAGE_SIZE, "0x%x\n", val);
}

static inline bool
etm4x_register_implemented(struct etmv4_drvdata *drvdata, u32 offset)
{
	switch (offset) {
	ETM_COMMON_SYSREG_LIST_CASES
		 
		return true;

	ETM4x_ONLY_SYSREG_LIST_CASES
		 
		return !etm4x_is_ete(drvdata);

	ETM4x_MMAP_LIST_CASES
		 
		return !!drvdata->base;

	ETE_ONLY_SYSREG_LIST_CASES
		return etm4x_is_ete(drvdata);
	}

	return false;
}

 
static umode_t
coresight_etm4x_attr_reg_implemented(struct kobject *kobj,
				     struct attribute *attr, int unused)
{
	struct device *dev = kobj_to_dev(kobj);
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct device_attribute *d_attr;
	u32 offset;

	d_attr = container_of(attr, struct device_attribute, attr);
	offset = coresight_etm4x_attr_to_offset(d_attr);

	if (etm4x_register_implemented(drvdata, offset))
		return attr->mode;
	return 0;
}

 
#define coresight_etm4x_reg_showfn(name, offset, showfn) (	\
	&((struct dev_ext_attribute[]) {			\
	   {							\
		__ATTR(name, 0444, showfn, NULL),		\
		(void *)(unsigned long)offset			\
	   }							\
	})[0].attr.attr						\
	)

 
#define coresight_etm4x_reg(name, offset)	\
	coresight_etm4x_reg_showfn(name, offset, coresight_etm4x_reg_show)

static struct attribute *coresight_etmv4_mgmt_attrs[] = {
	coresight_etm4x_reg(trcpdcr, TRCPDCR),
	coresight_etm4x_reg(trcpdsr, TRCPDSR),
	coresight_etm4x_reg(trclsr, TRCLSR),
	coresight_etm4x_reg(trcauthstatus, TRCAUTHSTATUS),
	coresight_etm4x_reg(trcdevid, TRCDEVID),
	coresight_etm4x_reg(trcdevtype, TRCDEVTYPE),
	coresight_etm4x_reg(trcpidr0, TRCPIDR0),
	coresight_etm4x_reg(trcpidr1, TRCPIDR1),
	coresight_etm4x_reg(trcpidr2, TRCPIDR2),
	coresight_etm4x_reg(trcpidr3, TRCPIDR3),
	coresight_etm4x_reg(trcoslsr, TRCOSLSR),
	coresight_etm4x_reg(trcconfig, TRCCONFIGR),
	coresight_etm4x_reg_showfn(trctraceid, TRCTRACEIDR, trctraceid_show),
	coresight_etm4x_reg(trcdevarch, TRCDEVARCH),
	NULL,
};

static struct attribute *coresight_etmv4_trcidr_attrs[] = {
	coresight_etm4x_reg(trcidr0, TRCIDR0),
	coresight_etm4x_reg(trcidr1, TRCIDR1),
	coresight_etm4x_reg(trcidr2, TRCIDR2),
	coresight_etm4x_reg(trcidr3, TRCIDR3),
	coresight_etm4x_reg(trcidr4, TRCIDR4),
	coresight_etm4x_reg(trcidr5, TRCIDR5),
	 
	coresight_etm4x_reg(trcidr8, TRCIDR8),
	coresight_etm4x_reg(trcidr9, TRCIDR9),
	coresight_etm4x_reg(trcidr10, TRCIDR10),
	coresight_etm4x_reg(trcidr11, TRCIDR11),
	coresight_etm4x_reg(trcidr12, TRCIDR12),
	coresight_etm4x_reg(trcidr13, TRCIDR13),
	NULL,
};

static const struct attribute_group coresight_etmv4_group = {
	.attrs = coresight_etmv4_attrs,
};

static const struct attribute_group coresight_etmv4_mgmt_group = {
	.is_visible = coresight_etm4x_attr_reg_implemented,
	.attrs = coresight_etmv4_mgmt_attrs,
	.name = "mgmt",
};

static const struct attribute_group coresight_etmv4_trcidr_group = {
	.attrs = coresight_etmv4_trcidr_attrs,
	.name = "trcidr",
};

const struct attribute_group *coresight_etmv4_groups[] = {
	&coresight_etmv4_group,
	&coresight_etmv4_mgmt_group,
	&coresight_etmv4_trcidr_group,
	NULL,
};
