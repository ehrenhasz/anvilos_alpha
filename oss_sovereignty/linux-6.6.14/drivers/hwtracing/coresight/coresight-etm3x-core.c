
 

#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/pm_runtime.h>
#include <linux/cpu.h>
#include <linux/of.h>
#include <linux/coresight.h>
#include <linux/coresight-pmu.h>
#include <linux/amba/bus.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/perf_event.h>
#include <asm/sections.h>

#include "coresight-etm.h"
#include "coresight-etm-perf.h"
#include "coresight-trace-id.h"

 
static int boot_enable;
module_param_named(boot_enable, boot_enable, int, S_IRUGO);

static struct etm_drvdata *etmdrvdata[NR_CPUS];

static enum cpuhp_state hp_online;

 
static void etm_os_unlock(struct etm_drvdata *drvdata)
{
	 
	etm_writel(drvdata, 0x0, ETMOSLAR);
	drvdata->os_unlock = true;
	isb();
}

static void etm_set_pwrdwn(struct etm_drvdata *drvdata)
{
	u32 etmcr;

	 
	mb();
	isb();
	etmcr = etm_readl(drvdata, ETMCR);
	etmcr |= ETMCR_PWD_DWN;
	etm_writel(drvdata, etmcr, ETMCR);
}

static void etm_clr_pwrdwn(struct etm_drvdata *drvdata)
{
	u32 etmcr;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr &= ~ETMCR_PWD_DWN;
	etm_writel(drvdata, etmcr, ETMCR);
	 
	mb();
	isb();
}

static void etm_set_pwrup(struct etm_drvdata *drvdata)
{
	u32 etmpdcr;

	etmpdcr = readl_relaxed(drvdata->base + ETMPDCR);
	etmpdcr |= ETMPDCR_PWD_UP;
	writel_relaxed(etmpdcr, drvdata->base + ETMPDCR);
	 
	mb();
	isb();
}

static void etm_clr_pwrup(struct etm_drvdata *drvdata)
{
	u32 etmpdcr;

	 
	mb();
	isb();
	etmpdcr = readl_relaxed(drvdata->base + ETMPDCR);
	etmpdcr &= ~ETMPDCR_PWD_UP;
	writel_relaxed(etmpdcr, drvdata->base + ETMPDCR);
}

 

static int coresight_timeout_etm(struct etm_drvdata *drvdata, u32 offset,
				  int position, int value)
{
	int i;
	u32 val;

	for (i = TIMEOUT_US; i > 0; i--) {
		val = etm_readl(drvdata, offset);
		 
		if (value) {
			if (val & BIT(position))
				return 0;
		 
		} else {
			if (!(val & BIT(position)))
				return 0;
		}

		 
		if (i - 1)
			udelay(1);
	}

	return -EAGAIN;
}


static void etm_set_prog(struct etm_drvdata *drvdata)
{
	u32 etmcr;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr |= ETMCR_ETM_PRG;
	etm_writel(drvdata, etmcr, ETMCR);
	 
	isb();
	if (coresight_timeout_etm(drvdata, ETMSR, ETMSR_PROG_BIT, 1)) {
		dev_err(&drvdata->csdev->dev,
			"%s: timeout observed when probing at offset %#x\n",
			__func__, ETMSR);
	}
}

static void etm_clr_prog(struct etm_drvdata *drvdata)
{
	u32 etmcr;

	etmcr = etm_readl(drvdata, ETMCR);
	etmcr &= ~ETMCR_ETM_PRG;
	etm_writel(drvdata, etmcr, ETMCR);
	 
	isb();
	if (coresight_timeout_etm(drvdata, ETMSR, ETMSR_PROG_BIT, 0)) {
		dev_err(&drvdata->csdev->dev,
			"%s: timeout observed when probing at offset %#x\n",
			__func__, ETMSR);
	}
}

void etm_set_default(struct etm_config *config)
{
	int i;

	if (WARN_ON_ONCE(!config))
		return;

	 
	config->enable_ctrl1 = ETMTECR1_INC_EXC;
	config->enable_ctrl2 = 0x0;
	config->enable_event = ETM_HARD_WIRE_RES_A;

	config->trigger_event = ETM_DEFAULT_EVENT_VAL;
	config->enable_event = ETM_HARD_WIRE_RES_A;

	config->seq_12_event = ETM_DEFAULT_EVENT_VAL;
	config->seq_21_event = ETM_DEFAULT_EVENT_VAL;
	config->seq_23_event = ETM_DEFAULT_EVENT_VAL;
	config->seq_31_event = ETM_DEFAULT_EVENT_VAL;
	config->seq_32_event = ETM_DEFAULT_EVENT_VAL;
	config->seq_13_event = ETM_DEFAULT_EVENT_VAL;
	config->timestamp_event = ETM_DEFAULT_EVENT_VAL;

	for (i = 0; i < ETM_MAX_CNTR; i++) {
		config->cntr_rld_val[i] = 0x0;
		config->cntr_event[i] = ETM_DEFAULT_EVENT_VAL;
		config->cntr_rld_event[i] = ETM_DEFAULT_EVENT_VAL;
		config->cntr_val[i] = 0x0;
	}

	config->seq_curr_state = 0x0;
	config->ctxid_idx = 0x0;
	for (i = 0; i < ETM_MAX_CTXID_CMP; i++)
		config->ctxid_pid[i] = 0x0;

	config->ctxid_mask = 0x0;
	 
	config->sync_freq = 0x400;
}

void etm_config_trace_mode(struct etm_config *config)
{
	u32 flags, mode;

	mode = config->mode;

	mode &= (ETM_MODE_EXCL_KERN | ETM_MODE_EXCL_USER);

	 
	if (mode == (ETM_MODE_EXCL_KERN | ETM_MODE_EXCL_USER))
		return;

	 
	if (!(mode & ETM_MODE_EXCL_KERN) && !(mode & ETM_MODE_EXCL_USER))
		return;

	flags = (1 << 0 |	 
		 3 << 3 |	 
		 0 << 5 |	 
		 0 << 7 |	 
		 0 << 8);	 

	 
	config->enable_ctrl2 = 0x0;

	 
	config->enable_ctrl1 = ETMTECR1_ADDR_COMP_1;

	 

	 
	flags |= (0 << 12 | 1 << 10);

	if (mode & ETM_MODE_EXCL_USER) {
		 
		flags |= (1 << 13 | 0 << 11);
	} else {
		 
		flags |= (1 << 13 | 1 << 11);
	}

	 
	config->addr_val[0] = (u32) 0x0;
	config->addr_val[1] = (u32) ~0x0;
	config->addr_acctype[0] = flags;
	config->addr_acctype[1] = flags;
	config->addr_type[0] = ETM_ADDR_TYPE_RANGE;
	config->addr_type[1] = ETM_ADDR_TYPE_RANGE;
}

#define ETM3X_SUPPORTED_OPTIONS (ETMCR_CYC_ACC | \
				 ETMCR_TIMESTAMP_EN | \
				 ETMCR_RETURN_STACK)

static int etm_parse_event_config(struct etm_drvdata *drvdata,
				  struct perf_event *event)
{
	struct etm_config *config = &drvdata->config;
	struct perf_event_attr *attr = &event->attr;

	if (!attr)
		return -EINVAL;

	 
	memset(config, 0, sizeof(struct etm_config));

	if (attr->exclude_kernel)
		config->mode = ETM_MODE_EXCL_KERN;

	if (attr->exclude_user)
		config->mode = ETM_MODE_EXCL_USER;

	 
	etm_set_default(config);

	 
	if (config->mode)
		etm_config_trace_mode(config);

	 
	if (attr->config & ~ETM3X_SUPPORTED_OPTIONS)
		return -EINVAL;

	config->ctrl = attr->config;

	 
	if (!task_is_in_init_pid_ns(current))
		config->ctrl &= ~ETMCR_CTXID_SIZE;

	 
	if ((config->ctrl & ETMCR_RETURN_STACK) &&
	    !(drvdata->etmccer & ETMCCER_RETSTACK))
		config->ctrl &= ~ETMCR_RETURN_STACK;

	return 0;
}

static int etm_enable_hw(struct etm_drvdata *drvdata)
{
	int i, rc;
	u32 etmcr;
	struct etm_config *config = &drvdata->config;
	struct coresight_device *csdev = drvdata->csdev;

	CS_UNLOCK(drvdata->base);

	rc = coresight_claim_device_unlocked(csdev);
	if (rc)
		goto done;

	 
	etm_clr_pwrdwn(drvdata);
	 
	etm_set_pwrup(drvdata);
	 
	etm_os_unlock(drvdata);

	etm_set_prog(drvdata);

	etmcr = etm_readl(drvdata, ETMCR);
	 
	etmcr &= ~ETM3X_SUPPORTED_OPTIONS;
	etmcr |= drvdata->port_size;
	etmcr |= ETMCR_ETM_EN;
	etm_writel(drvdata, config->ctrl | etmcr, ETMCR);
	etm_writel(drvdata, config->trigger_event, ETMTRIGGER);
	etm_writel(drvdata, config->startstop_ctrl, ETMTSSCR);
	etm_writel(drvdata, config->enable_event, ETMTEEVR);
	etm_writel(drvdata, config->enable_ctrl1, ETMTECR1);
	etm_writel(drvdata, config->fifofull_level, ETMFFLR);
	for (i = 0; i < drvdata->nr_addr_cmp; i++) {
		etm_writel(drvdata, config->addr_val[i], ETMACVRn(i));
		etm_writel(drvdata, config->addr_acctype[i], ETMACTRn(i));
	}
	for (i = 0; i < drvdata->nr_cntr; i++) {
		etm_writel(drvdata, config->cntr_rld_val[i], ETMCNTRLDVRn(i));
		etm_writel(drvdata, config->cntr_event[i], ETMCNTENRn(i));
		etm_writel(drvdata, config->cntr_rld_event[i],
			   ETMCNTRLDEVRn(i));
		etm_writel(drvdata, config->cntr_val[i], ETMCNTVRn(i));
	}
	etm_writel(drvdata, config->seq_12_event, ETMSQ12EVR);
	etm_writel(drvdata, config->seq_21_event, ETMSQ21EVR);
	etm_writel(drvdata, config->seq_23_event, ETMSQ23EVR);
	etm_writel(drvdata, config->seq_31_event, ETMSQ31EVR);
	etm_writel(drvdata, config->seq_32_event, ETMSQ32EVR);
	etm_writel(drvdata, config->seq_13_event, ETMSQ13EVR);
	etm_writel(drvdata, config->seq_curr_state, ETMSQR);
	for (i = 0; i < drvdata->nr_ext_out; i++)
		etm_writel(drvdata, ETM_DEFAULT_EVENT_VAL, ETMEXTOUTEVRn(i));
	for (i = 0; i < drvdata->nr_ctxid_cmp; i++)
		etm_writel(drvdata, config->ctxid_pid[i], ETMCIDCVRn(i));
	etm_writel(drvdata, config->ctxid_mask, ETMCIDCMR);
	etm_writel(drvdata, config->sync_freq, ETMSYNCFR);
	 
	etm_writel(drvdata, 0x0, ETMEXTINSELR);
	etm_writel(drvdata, config->timestamp_event, ETMTSEVR);
	 
	etm_writel(drvdata, 0x0, ETMAUXCR);
	etm_writel(drvdata, drvdata->traceid, ETMTRACEIDR);
	 
	etm_writel(drvdata, 0x0, ETMVMIDCVR);

	etm_clr_prog(drvdata);

done:
	CS_LOCK(drvdata->base);

	dev_dbg(&drvdata->csdev->dev, "cpu: %d enable smp call done: %d\n",
		drvdata->cpu, rc);
	return rc;
}

struct etm_enable_arg {
	struct etm_drvdata *drvdata;
	int rc;
};

static void etm_enable_hw_smp_call(void *info)
{
	struct etm_enable_arg *arg = info;

	if (WARN_ON(!arg))
		return;
	arg->rc = etm_enable_hw(arg->drvdata);
}

static int etm_cpu_id(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return drvdata->cpu;
}

int etm_read_alloc_trace_id(struct etm_drvdata *drvdata)
{
	int trace_id;

	 
	trace_id = coresight_trace_id_get_cpu_id(drvdata->cpu);
	if (IS_VALID_CS_TRACE_ID(trace_id))
		drvdata->traceid = (u8)trace_id;
	else
		dev_err(&drvdata->csdev->dev,
			"Failed to allocate trace ID for %s on CPU%d\n",
			dev_name(&drvdata->csdev->dev), drvdata->cpu);
	return trace_id;
}

void etm_release_trace_id(struct etm_drvdata *drvdata)
{
	coresight_trace_id_put_cpu_id(drvdata->cpu);
}

static int etm_enable_perf(struct coresight_device *csdev,
			   struct perf_event *event)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	int trace_id;

	if (WARN_ON_ONCE(drvdata->cpu != smp_processor_id()))
		return -EINVAL;

	 
	etm_parse_event_config(drvdata, event);

	 
	trace_id = coresight_trace_id_read_cpu_id(drvdata->cpu);
	if (!IS_VALID_CS_TRACE_ID(trace_id)) {
		dev_err(&drvdata->csdev->dev, "Failed to set trace ID for %s on CPU%d\n",
			dev_name(&drvdata->csdev->dev), drvdata->cpu);
		return -EINVAL;
	}
	drvdata->traceid = (u8)trace_id;

	 
	return etm_enable_hw(drvdata);
}

static int etm_enable_sysfs(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etm_enable_arg arg = { };
	int ret;

	spin_lock(&drvdata->spinlock);

	 
	ret = etm_read_alloc_trace_id(drvdata);
	if (ret < 0)
		goto unlock_enable_sysfs;

	 
	if (cpu_online(drvdata->cpu)) {
		arg.drvdata = drvdata;
		ret = smp_call_function_single(drvdata->cpu,
					       etm_enable_hw_smp_call, &arg, 1);
		if (!ret)
			ret = arg.rc;
		if (!ret)
			drvdata->sticky_enable = true;
	} else {
		ret = -ENODEV;
	}

	if (ret)
		etm_release_trace_id(drvdata);

unlock_enable_sysfs:
	spin_unlock(&drvdata->spinlock);

	if (!ret)
		dev_dbg(&csdev->dev, "ETM tracing enabled\n");
	return ret;
}

static int etm_enable(struct coresight_device *csdev, struct perf_event *event,
		      enum cs_mode mode)
{
	int ret;
	u32 val;
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	val = local_cmpxchg(&drvdata->mode, CS_MODE_DISABLED, mode);

	 
	if (val)
		return -EBUSY;

	switch (mode) {
	case CS_MODE_SYSFS:
		ret = etm_enable_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		ret = etm_enable_perf(csdev, event);
		break;
	default:
		ret = -EINVAL;
	}

	 
	if (ret)
		local_set(&drvdata->mode, CS_MODE_DISABLED);

	return ret;
}

static void etm_disable_hw(void *info)
{
	int i;
	struct etm_drvdata *drvdata = info;
	struct etm_config *config = &drvdata->config;
	struct coresight_device *csdev = drvdata->csdev;

	CS_UNLOCK(drvdata->base);
	etm_set_prog(drvdata);

	 
	config->seq_curr_state = (etm_readl(drvdata, ETMSQR) & ETM_SQR_MASK);

	for (i = 0; i < drvdata->nr_cntr; i++)
		config->cntr_val[i] = etm_readl(drvdata, ETMCNTVRn(i));

	etm_set_pwrdwn(drvdata);
	coresight_disclaim_device_unlocked(csdev);

	CS_LOCK(drvdata->base);

	dev_dbg(&drvdata->csdev->dev,
		"cpu: %d disable smp call done\n", drvdata->cpu);
}

static void etm_disable_perf(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	if (WARN_ON_ONCE(drvdata->cpu != smp_processor_id()))
		return;

	CS_UNLOCK(drvdata->base);

	 
	etm_set_prog(drvdata);

	 
	etm_set_pwrdwn(drvdata);
	coresight_disclaim_device_unlocked(csdev);

	CS_LOCK(drvdata->base);

	 

}

static void etm_disable_sysfs(struct coresight_device *csdev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	 
	cpus_read_lock();
	spin_lock(&drvdata->spinlock);

	 
	smp_call_function_single(drvdata->cpu, etm_disable_hw, drvdata, 1);

	spin_unlock(&drvdata->spinlock);
	cpus_read_unlock();

	 

	dev_dbg(&csdev->dev, "ETM tracing disabled\n");
}

static void etm_disable(struct coresight_device *csdev,
			struct perf_event *event)
{
	enum cs_mode mode;
	struct etm_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	 
	mode = local_read(&drvdata->mode);

	switch (mode) {
	case CS_MODE_DISABLED:
		break;
	case CS_MODE_SYSFS:
		etm_disable_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		etm_disable_perf(csdev);
		break;
	default:
		WARN_ON_ONCE(mode);
		return;
	}

	if (mode)
		local_set(&drvdata->mode, CS_MODE_DISABLED);
}

static const struct coresight_ops_source etm_source_ops = {
	.cpu_id		= etm_cpu_id,
	.enable		= etm_enable,
	.disable	= etm_disable,
};

static const struct coresight_ops etm_cs_ops = {
	.source_ops	= &etm_source_ops,
};

static int etm_online_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return 0;

	if (etmdrvdata[cpu]->boot_enable && !etmdrvdata[cpu]->sticky_enable)
		coresight_enable(etmdrvdata[cpu]->csdev);
	return 0;
}

static int etm_starting_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return 0;

	spin_lock(&etmdrvdata[cpu]->spinlock);
	if (!etmdrvdata[cpu]->os_unlock) {
		etm_os_unlock(etmdrvdata[cpu]);
		etmdrvdata[cpu]->os_unlock = true;
	}

	if (local_read(&etmdrvdata[cpu]->mode))
		etm_enable_hw(etmdrvdata[cpu]);
	spin_unlock(&etmdrvdata[cpu]->spinlock);
	return 0;
}

static int etm_dying_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return 0;

	spin_lock(&etmdrvdata[cpu]->spinlock);
	if (local_read(&etmdrvdata[cpu]->mode))
		etm_disable_hw(etmdrvdata[cpu]);
	spin_unlock(&etmdrvdata[cpu]->spinlock);
	return 0;
}

static bool etm_arch_supported(u8 arch)
{
	switch (arch) {
	case ETM_ARCH_V3_3:
		break;
	case ETM_ARCH_V3_5:
		break;
	case PFT_ARCH_V1_0:
		break;
	case PFT_ARCH_V1_1:
		break;
	default:
		return false;
	}
	return true;
}

static void etm_init_arch_data(void *info)
{
	u32 etmidr;
	u32 etmccr;
	struct etm_drvdata *drvdata = info;

	 
	etm_os_unlock(drvdata);

	CS_UNLOCK(drvdata->base);

	 
	(void)etm_readl(drvdata, ETMPDSR);
	 
	etm_set_pwrup(drvdata);
	 
	etm_clr_pwrdwn(drvdata);
	 
	etm_set_prog(drvdata);

	 
	etmidr = etm_readl(drvdata, ETMIDR);
	drvdata->arch = BMVAL(etmidr, 4, 11);
	drvdata->port_size = etm_readl(drvdata, ETMCR) & PORT_SIZE_MASK;

	drvdata->etmccer = etm_readl(drvdata, ETMCCER);
	etmccr = etm_readl(drvdata, ETMCCR);
	drvdata->etmccr = etmccr;
	drvdata->nr_addr_cmp = BMVAL(etmccr, 0, 3) * 2;
	drvdata->nr_cntr = BMVAL(etmccr, 13, 15);
	drvdata->nr_ext_inp = BMVAL(etmccr, 17, 19);
	drvdata->nr_ext_out = BMVAL(etmccr, 20, 22);
	drvdata->nr_ctxid_cmp = BMVAL(etmccr, 24, 25);

	etm_set_pwrdwn(drvdata);
	etm_clr_pwrup(drvdata);
	CS_LOCK(drvdata->base);
}

static int __init etm_hp_setup(void)
{
	int ret;

	ret = cpuhp_setup_state_nocalls_cpuslocked(CPUHP_AP_ARM_CORESIGHT_STARTING,
						   "arm/coresight:starting",
						   etm_starting_cpu, etm_dying_cpu);

	if (ret)
		return ret;

	ret = cpuhp_setup_state_nocalls_cpuslocked(CPUHP_AP_ONLINE_DYN,
						   "arm/coresight:online",
						   etm_online_cpu, NULL);

	 
	if (ret > 0) {
		hp_online = ret;
		return 0;
	}

	 
	cpuhp_remove_state_nocalls(CPUHP_AP_ARM_CORESIGHT_STARTING);

	return ret;
}

static void etm_hp_clear(void)
{
	cpuhp_remove_state_nocalls(CPUHP_AP_ARM_CORESIGHT_STARTING);
	if (hp_online) {
		cpuhp_remove_state_nocalls(hp_online);
		hp_online = 0;
	}
}

static int etm_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct coresight_platform_data *pdata = NULL;
	struct etm_drvdata *drvdata;
	struct resource *res = &adev->res;
	struct coresight_desc desc = { 0 };

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->use_cp14 = fwnode_property_read_bool(dev->fwnode, "arm,cp14");
	dev_set_drvdata(dev, drvdata);

	 
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	drvdata->base = base;
	desc.access = CSDEV_ACCESS_IOMEM(base);

	spin_lock_init(&drvdata->spinlock);

	drvdata->atclk = devm_clk_get(&adev->dev, "atclk");  
	if (!IS_ERR(drvdata->atclk)) {
		ret = clk_prepare_enable(drvdata->atclk);
		if (ret)
			return ret;
	}

	drvdata->cpu = coresight_get_cpu(dev);
	if (drvdata->cpu < 0)
		return drvdata->cpu;

	desc.name  = devm_kasprintf(dev, GFP_KERNEL, "etm%d", drvdata->cpu);
	if (!desc.name)
		return -ENOMEM;

	if (smp_call_function_single(drvdata->cpu,
				     etm_init_arch_data,  drvdata, 1))
		dev_err(dev, "ETM arch init failed\n");

	if (etm_arch_supported(drvdata->arch) == false)
		return -EINVAL;

	etm_set_default(&drvdata->config);

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	adev->dev.platform_data = pdata;

	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc.ops = &etm_cs_ops;
	desc.pdata = pdata;
	desc.dev = dev;
	desc.groups = coresight_etm_groups;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	ret = etm_perf_symlink(drvdata->csdev, true);
	if (ret) {
		coresight_unregister(drvdata->csdev);
		return ret;
	}

	etmdrvdata[drvdata->cpu] = drvdata;

	pm_runtime_put(&adev->dev);
	dev_info(&drvdata->csdev->dev,
		 "%s initialized\n", (char *)coresight_get_uci_data(id));
	if (boot_enable) {
		coresight_enable(drvdata->csdev);
		drvdata->boot_enable = true;
	}

	return 0;
}

static void clear_etmdrvdata(void *info)
{
	int cpu = *(int *)info;

	etmdrvdata[cpu] = NULL;
}

static void etm_remove(struct amba_device *adev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	etm_perf_symlink(drvdata->csdev, false);

	 
	cpus_read_lock();
	 
	if (smp_call_function_single(drvdata->cpu, clear_etmdrvdata, &drvdata->cpu, 1))
		etmdrvdata[drvdata->cpu] = NULL;

	cpus_read_unlock();

	coresight_unregister(drvdata->csdev);
}

#ifdef CONFIG_PM
static int etm_runtime_suspend(struct device *dev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_disable_unprepare(drvdata->atclk);

	return 0;
}

static int etm_runtime_resume(struct device *dev)
{
	struct etm_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata && !IS_ERR(drvdata->atclk))
		clk_prepare_enable(drvdata->atclk);

	return 0;
}
#endif

static const struct dev_pm_ops etm_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(etm_runtime_suspend, etm_runtime_resume, NULL)
};

static const struct amba_id etm_ids[] = {
	 
	CS_AMBA_ID_DATA(0x000bb921, "ETM 3.3"),
	 
	CS_AMBA_ID_DATA(0x000bb955, "ETM 3.5"),
	 
	CS_AMBA_ID_DATA(0x000bb956, "ETM 3.5"),
	 
	CS_AMBA_ID_DATA(0x000bb950, "PTM 1.0"),
	 
	CS_AMBA_ID_DATA(0x000bb95f, "PTM 1.1"),
	 
	CS_AMBA_ID_DATA(0x000b006f, "PTM 1.1"),
	{ 0, 0},
};

MODULE_DEVICE_TABLE(amba, etm_ids);

static struct amba_driver etm_driver = {
	.drv = {
		.name	= "coresight-etm3x",
		.owner	= THIS_MODULE,
		.pm	= &etm_dev_pm_ops,
		.suppress_bind_attrs = true,
	},
	.probe		= etm_probe,
	.remove         = etm_remove,
	.id_table	= etm_ids,
};

static int __init etm_init(void)
{
	int ret;

	ret = etm_hp_setup();

	 
	if (ret)
		return ret;

	ret = amba_driver_register(&etm_driver);
	if (ret) {
		pr_err("Error registering etm3x driver\n");
		etm_hp_clear();
	}

	return ret;
}

static void __exit etm_exit(void)
{
	amba_driver_unregister(&etm_driver);
	etm_hp_clear();
}

module_init(etm_init);
module_exit(etm_exit);

MODULE_AUTHOR("Pratik Patel <pratikp@codeaurora.org>");
MODULE_AUTHOR("Mathieu Poirier <mathieu.poirier@linaro.org>");
MODULE_DESCRIPTION("Arm CoreSight Program Flow Trace driver");
MODULE_LICENSE("GPL v2");
