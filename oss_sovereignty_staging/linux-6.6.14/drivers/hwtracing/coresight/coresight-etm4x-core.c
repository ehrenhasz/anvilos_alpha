
 

#include <linux/acpi.h>
#include <linux/bitops.h>
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
#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include <linux/coresight.h>
#include <linux/coresight-pmu.h>
#include <linux/pm_wakeup.h>
#include <linux/amba/bus.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/perf_event.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/property.h>
#include <linux/clk/clk-conf.h>

#include <asm/barrier.h>
#include <asm/sections.h>
#include <asm/sysreg.h>
#include <asm/local.h>
#include <asm/virt.h>

#include "coresight-etm4x.h"
#include "coresight-etm-perf.h"
#include "coresight-etm4x-cfg.h"
#include "coresight-self-hosted-trace.h"
#include "coresight-syscfg.h"
#include "coresight-trace-id.h"

static int boot_enable;
module_param(boot_enable, int, 0444);
MODULE_PARM_DESC(boot_enable, "Enable tracing on boot");

#define PARAM_PM_SAVE_FIRMWARE	  0  
#define PARAM_PM_SAVE_NEVER	  1  
#define PARAM_PM_SAVE_SELF_HOSTED 2  

static int pm_save_enable = PARAM_PM_SAVE_FIRMWARE;
module_param(pm_save_enable, int, 0444);
MODULE_PARM_DESC(pm_save_enable,
	"Save/restore state on power down: 1 = never, 2 = self-hosted");

static struct etmv4_drvdata *etmdrvdata[NR_CPUS];
static void etm4_set_default_config(struct etmv4_config *config);
static int etm4_set_event_filters(struct etmv4_drvdata *drvdata,
				  struct perf_event *event);
static u64 etm4_get_access_type(struct etmv4_config *config);

static enum cpuhp_state hp_online;

struct etm4_init_arg {
	struct device		*dev;
	struct csdev_access	*csa;
};

static DEFINE_PER_CPU(struct etm4_init_arg *, delayed_probe);
static int etm4_probe_cpu(unsigned int cpu);

 
static inline bool etm4x_sspcicrn_present(struct etmv4_drvdata *drvdata, int n)
{
	return (n < drvdata->nr_ss_cmp) &&
	       drvdata->nr_pe &&
	       (drvdata->config.ss_status[n] & TRCSSCSRn_PC);
}

u64 etm4x_sysreg_read(u32 offset, bool _relaxed, bool _64bit)
{
	u64 res = 0;

	switch (offset) {
	ETM4x_READ_SYSREG_CASES(res)
	default :
		pr_warn_ratelimited("etm4x: trying to read unsupported register @%x\n",
			 offset);
	}

	if (!_relaxed)
		__io_ar(res);	 

	return res;
}

void etm4x_sysreg_write(u64 val, u32 offset, bool _relaxed, bool _64bit)
{
	if (!_relaxed)
		__io_bw();	 
	if (!_64bit)
		val &= GENMASK(31, 0);

	switch (offset) {
	ETM4x_WRITE_SYSREG_CASES(val)
	default :
		pr_warn_ratelimited("etm4x: trying to write to unsupported register @%x\n",
			offset);
	}
}

static u64 ete_sysreg_read(u32 offset, bool _relaxed, bool _64bit)
{
	u64 res = 0;

	switch (offset) {
	ETE_READ_CASES(res)
	default :
		pr_warn_ratelimited("ete: trying to read unsupported register @%x\n",
				    offset);
	}

	if (!_relaxed)
		__io_ar(res);	 

	return res;
}

static void ete_sysreg_write(u64 val, u32 offset, bool _relaxed, bool _64bit)
{
	if (!_relaxed)
		__io_bw();	 
	if (!_64bit)
		val &= GENMASK(31, 0);

	switch (offset) {
	ETE_WRITE_CASES(val)
	default :
		pr_warn_ratelimited("ete: trying to write to unsupported register @%x\n",
				    offset);
	}
}

static void etm_detect_os_lock(struct etmv4_drvdata *drvdata,
			       struct csdev_access *csa)
{
	u32 oslsr = etm4x_relaxed_read32(csa, TRCOSLSR);

	drvdata->os_lock_model = ETM_OSLSR_OSLM(oslsr);
}

static void etm_write_os_lock(struct etmv4_drvdata *drvdata,
			      struct csdev_access *csa, u32 val)
{
	val = !!val;

	switch (drvdata->os_lock_model) {
	case ETM_OSLOCK_PRESENT:
		etm4x_relaxed_write32(csa, val, TRCOSLAR);
		break;
	case ETM_OSLOCK_PE:
		write_sysreg_s(val, SYS_OSLAR_EL1);
		break;
	default:
		pr_warn_once("CPU%d: Unsupported Trace OSLock model: %x\n",
			     smp_processor_id(), drvdata->os_lock_model);
		fallthrough;
	case ETM_OSLOCK_NI:
		return;
	}
	isb();
}

static inline void etm4_os_unlock_csa(struct etmv4_drvdata *drvdata,
				      struct csdev_access *csa)
{
	WARN_ON(drvdata->cpu != smp_processor_id());

	 
	etm_write_os_lock(drvdata, csa, 0x0);
	drvdata->os_unlock = true;
}

static void etm4_os_unlock(struct etmv4_drvdata *drvdata)
{
	if (!WARN_ON(!drvdata->csdev))
		etm4_os_unlock_csa(drvdata, &drvdata->csdev->access);
}

static void etm4_os_lock(struct etmv4_drvdata *drvdata)
{
	if (WARN_ON(!drvdata->csdev))
		return;
	 
	etm_write_os_lock(drvdata, &drvdata->csdev->access, 0x1);
	drvdata->os_unlock = false;
}

static void etm4_cs_lock(struct etmv4_drvdata *drvdata,
			 struct csdev_access *csa)
{
	 
	if (csa->io_mem)
		CS_LOCK(csa->base);
}

static void etm4_cs_unlock(struct etmv4_drvdata *drvdata,
			   struct csdev_access *csa)
{
	if (csa->io_mem)
		CS_UNLOCK(csa->base);
}

static int etm4_cpu_id(struct coresight_device *csdev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	return drvdata->cpu;
}

int etm4_read_alloc_trace_id(struct etmv4_drvdata *drvdata)
{
	int trace_id;

	 
	trace_id = coresight_trace_id_get_cpu_id(drvdata->cpu);
	if (IS_VALID_CS_TRACE_ID(trace_id))
		drvdata->trcid = (u8)trace_id;
	else
		dev_err(&drvdata->csdev->dev,
			"Failed to allocate trace ID for %s on CPU%d\n",
			dev_name(&drvdata->csdev->dev), drvdata->cpu);
	return trace_id;
}

void etm4_release_trace_id(struct etmv4_drvdata *drvdata)
{
	coresight_trace_id_put_cpu_id(drvdata->cpu);
}

struct etm4_enable_arg {
	struct etmv4_drvdata *drvdata;
	int rc;
};

 
static void etm4x_prohibit_trace(struct etmv4_drvdata *drvdata)
{
	 
	if (!drvdata->trfcr)
		return;
	cpu_prohibit_trace();
}

 
static void etm4x_allow_trace(struct etmv4_drvdata *drvdata)
{
	u64 trfcr = drvdata->trfcr;

	 
	if (!trfcr)
		return;

	if (drvdata->config.mode & ETM_MODE_EXCL_KERN)
		trfcr &= ~TRFCR_ELx_ExTRE;
	if (drvdata->config.mode & ETM_MODE_EXCL_USER)
		trfcr &= ~TRFCR_ELx_E0TRE;

	write_trfcr(trfcr);
}

#ifdef CONFIG_ETM4X_IMPDEF_FEATURE

#define HISI_HIP08_AMBA_ID		0x000b6d01
#define ETM4_AMBA_MASK			0xfffff
#define HISI_HIP08_CORE_COMMIT_MASK	0x3000
#define HISI_HIP08_CORE_COMMIT_SHIFT	12
#define HISI_HIP08_CORE_COMMIT_FULL	0b00
#define HISI_HIP08_CORE_COMMIT_LVL_1	0b01
#define HISI_HIP08_CORE_COMMIT_REG	sys_reg(3, 1, 15, 2, 5)

struct etm4_arch_features {
	void (*arch_callback)(bool enable);
};

static bool etm4_hisi_match_pid(unsigned int id)
{
	return (id & ETM4_AMBA_MASK) == HISI_HIP08_AMBA_ID;
}

static void etm4_hisi_config_core_commit(bool enable)
{
	u8 commit = enable ? HISI_HIP08_CORE_COMMIT_LVL_1 :
		    HISI_HIP08_CORE_COMMIT_FULL;
	u64 val;

	 
	val = read_sysreg_s(HISI_HIP08_CORE_COMMIT_REG);
	val &= ~HISI_HIP08_CORE_COMMIT_MASK;
	val |= commit << HISI_HIP08_CORE_COMMIT_SHIFT;
	write_sysreg_s(val, HISI_HIP08_CORE_COMMIT_REG);
}

static struct etm4_arch_features etm4_features[] = {
	[ETM4_IMPDEF_HISI_CORE_COMMIT] = {
		.arch_callback = etm4_hisi_config_core_commit,
	},
	{},
};

static void etm4_enable_arch_specific(struct etmv4_drvdata *drvdata)
{
	struct etm4_arch_features *ftr;
	int bit;

	for_each_set_bit(bit, drvdata->arch_features, ETM4_IMPDEF_FEATURE_MAX) {
		ftr = &etm4_features[bit];

		if (ftr->arch_callback)
			ftr->arch_callback(true);
	}
}

static void etm4_disable_arch_specific(struct etmv4_drvdata *drvdata)
{
	struct etm4_arch_features *ftr;
	int bit;

	for_each_set_bit(bit, drvdata->arch_features, ETM4_IMPDEF_FEATURE_MAX) {
		ftr = &etm4_features[bit];

		if (ftr->arch_callback)
			ftr->arch_callback(false);
	}
}

static void etm4_check_arch_features(struct etmv4_drvdata *drvdata,
				     struct csdev_access *csa)
{
	 
	if (!csa->io_mem)
		return;

	if (etm4_hisi_match_pid(coresight_get_pid(csa)))
		set_bit(ETM4_IMPDEF_HISI_CORE_COMMIT, drvdata->arch_features);
}
#else
static void etm4_enable_arch_specific(struct etmv4_drvdata *drvdata)
{
}

static void etm4_disable_arch_specific(struct etmv4_drvdata *drvdata)
{
}

static void etm4_check_arch_features(struct etmv4_drvdata *drvdata,
				     struct csdev_access *csa)
{
}
#endif  

static int etm4_enable_hw(struct etmv4_drvdata *drvdata)
{
	int i, rc;
	struct etmv4_config *config = &drvdata->config;
	struct coresight_device *csdev = drvdata->csdev;
	struct device *etm_dev = &csdev->dev;
	struct csdev_access *csa = &csdev->access;


	etm4_cs_unlock(drvdata, csa);
	etm4_enable_arch_specific(drvdata);

	etm4_os_unlock(drvdata);

	rc = coresight_claim_device_unlocked(csdev);
	if (rc)
		goto done;

	 
	etm4x_relaxed_write32(csa, 0, TRCPRGCTLR);

	 
	if (!csa->io_mem)
		isb();

	 
	if (coresight_timeout(csa, TRCSTATR, TRCSTATR_IDLE_BIT, 1))
		dev_err(etm_dev,
			"timeout while waiting for Idle Trace Status\n");
	if (drvdata->nr_pe)
		etm4x_relaxed_write32(csa, config->pe_sel, TRCPROCSELR);
	etm4x_relaxed_write32(csa, config->cfg, TRCCONFIGR);
	 
	etm4x_relaxed_write32(csa, 0x0, TRCAUXCTLR);
	etm4x_relaxed_write32(csa, config->eventctrl0, TRCEVENTCTL0R);
	etm4x_relaxed_write32(csa, config->eventctrl1, TRCEVENTCTL1R);
	if (drvdata->stallctl)
		etm4x_relaxed_write32(csa, config->stall_ctrl, TRCSTALLCTLR);
	etm4x_relaxed_write32(csa, config->ts_ctrl, TRCTSCTLR);
	etm4x_relaxed_write32(csa, config->syncfreq, TRCSYNCPR);
	etm4x_relaxed_write32(csa, config->ccctlr, TRCCCCTLR);
	etm4x_relaxed_write32(csa, config->bb_ctrl, TRCBBCTLR);
	etm4x_relaxed_write32(csa, drvdata->trcid, TRCTRACEIDR);
	etm4x_relaxed_write32(csa, config->vinst_ctrl, TRCVICTLR);
	etm4x_relaxed_write32(csa, config->viiectlr, TRCVIIECTLR);
	etm4x_relaxed_write32(csa, config->vissctlr, TRCVISSCTLR);
	if (drvdata->nr_pe_cmp)
		etm4x_relaxed_write32(csa, config->vipcssctlr, TRCVIPCSSCTLR);
	for (i = 0; i < drvdata->nrseqstate - 1; i++)
		etm4x_relaxed_write32(csa, config->seq_ctrl[i], TRCSEQEVRn(i));
	if (drvdata->nrseqstate) {
		etm4x_relaxed_write32(csa, config->seq_rst, TRCSEQRSTEVR);
		etm4x_relaxed_write32(csa, config->seq_state, TRCSEQSTR);
	}
	etm4x_relaxed_write32(csa, config->ext_inp, TRCEXTINSELR);
	for (i = 0; i < drvdata->nr_cntr; i++) {
		etm4x_relaxed_write32(csa, config->cntrldvr[i], TRCCNTRLDVRn(i));
		etm4x_relaxed_write32(csa, config->cntr_ctrl[i], TRCCNTCTLRn(i));
		etm4x_relaxed_write32(csa, config->cntr_val[i], TRCCNTVRn(i));
	}

	 
	for (i = 2; i < drvdata->nr_resource * 2; i++)
		etm4x_relaxed_write32(csa, config->res_ctrl[i], TRCRSCTLRn(i));

	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		 
		if (config->ss_ctrl[i] || config->ss_pe_cmp[i])
			config->ss_status[i] &= ~TRCSSCSRn_STATUS;
		etm4x_relaxed_write32(csa, config->ss_ctrl[i], TRCSSCCRn(i));
		etm4x_relaxed_write32(csa, config->ss_status[i], TRCSSCSRn(i));
		if (etm4x_sspcicrn_present(drvdata, i))
			etm4x_relaxed_write32(csa, config->ss_pe_cmp[i], TRCSSPCICRn(i));
	}
	for (i = 0; i < drvdata->nr_addr_cmp * 2; i++) {
		etm4x_relaxed_write64(csa, config->addr_val[i], TRCACVRn(i));
		etm4x_relaxed_write64(csa, config->addr_acc[i], TRCACATRn(i));
	}
	for (i = 0; i < drvdata->numcidc; i++)
		etm4x_relaxed_write64(csa, config->ctxid_pid[i], TRCCIDCVRn(i));
	etm4x_relaxed_write32(csa, config->ctxid_mask0, TRCCIDCCTLR0);
	if (drvdata->numcidc > 4)
		etm4x_relaxed_write32(csa, config->ctxid_mask1, TRCCIDCCTLR1);

	for (i = 0; i < drvdata->numvmidc; i++)
		etm4x_relaxed_write64(csa, config->vmid_val[i], TRCVMIDCVRn(i));
	etm4x_relaxed_write32(csa, config->vmid_mask0, TRCVMIDCCTLR0);
	if (drvdata->numvmidc > 4)
		etm4x_relaxed_write32(csa, config->vmid_mask1, TRCVMIDCCTLR1);

	if (!drvdata->skip_power_up) {
		u32 trcpdcr = etm4x_relaxed_read32(csa, TRCPDCR);

		 
		etm4x_relaxed_write32(csa, trcpdcr | TRCPDCR_PU, TRCPDCR);
	}

	 
	if (etm4x_is_ete(drvdata))
		etm4x_relaxed_write32(csa, TRCRSR_TA, TRCRSR);

	etm4x_allow_trace(drvdata);
	 
	etm4x_relaxed_write32(csa, 1, TRCPRGCTLR);

	 
	if (!csa->io_mem)
		isb();

	 
	if (coresight_timeout(csa, TRCSTATR, TRCSTATR_IDLE_BIT, 0))
		dev_err(etm_dev,
			"timeout while waiting for Idle Trace Status\n");

	 
	dsb(sy);
	isb();

done:
	etm4_cs_lock(drvdata, csa);

	dev_dbg(etm_dev, "cpu: %d enable smp call done: %d\n",
		drvdata->cpu, rc);
	return rc;
}

static void etm4_enable_hw_smp_call(void *info)
{
	struct etm4_enable_arg *arg = info;

	if (WARN_ON(!arg))
		return;
	arg->rc = etm4_enable_hw(arg->drvdata);
}

 
static int etm4_config_timestamp_event(struct etmv4_drvdata *drvdata)
{
	int ctridx, ret = -EINVAL;
	int counter, rselector;
	u32 val = 0;
	struct etmv4_config *config = &drvdata->config;

	 
	if (!drvdata->nr_cntr)
		goto out;

	 
	for (ctridx = 0; ctridx < drvdata->nr_cntr; ctridx++)
		if (config->cntr_val[ctridx] == 0)
			break;

	 
	if (ctridx == drvdata->nr_cntr) {
		pr_debug("%s: no available counter found\n", __func__);
		ret = -ENOSPC;
		goto out;
	}

	 
	for (rselector = 2; rselector < drvdata->nr_resource * 2; rselector++)
		if (!config->res_ctrl[rselector])
			break;

	if (rselector == drvdata->nr_resource * 2) {
		pr_debug("%s: no available resource selector found\n",
			 __func__);
		ret = -ENOSPC;
		goto out;
	}

	 
	counter = 1 << ctridx;

	 
	config->cntr_val[ctridx] = 1;
	config->cntrldvr[ctridx] = 1;

	 
	val =  0x1 << 16	|   
	       0x0 << 7		|   
	       0x1;		    

	config->cntr_ctrl[ctridx] = val;

	val = 0x2 << 16		|  
	      counter << 0;	   

	config->res_ctrl[rselector] = val;

	val = 0x0 << 7		|  
	      rselector;	   

	config->ts_ctrl = val;

	ret = 0;
out:
	return ret;
}

static int etm4_parse_event_config(struct coresight_device *csdev,
				   struct perf_event *event)
{
	int ret = 0;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etmv4_config *config = &drvdata->config;
	struct perf_event_attr *attr = &event->attr;
	unsigned long cfg_hash;
	int preset;

	 
	memset(config, 0, sizeof(struct etmv4_config));

	if (attr->exclude_kernel)
		config->mode = ETM_MODE_EXCL_KERN;

	if (attr->exclude_user)
		config->mode = ETM_MODE_EXCL_USER;

	 
	etm4_set_default_config(config);

	 
	ret = etm4_set_event_filters(drvdata, event);
	if (ret)
		goto out;

	 
	if (attr->config & BIT(ETM_OPT_CYCACC)) {
		config->cfg |= TRCCONFIGR_CCI;
		 
		config->ccctlr = ETM_CYC_THRESHOLD_DEFAULT;
	}
	if (attr->config & BIT(ETM_OPT_TS)) {
		 
		ret = etm4_config_timestamp_event(drvdata);

		 
		if (ret)
			goto out;

		 
		config->cfg |= TRCCONFIGR_TS;
	}

	 
	if ((attr->config & BIT(ETM_OPT_CTXTID)) &&
	    task_is_in_init_pid_ns(current))
		 
		config->cfg |= TRCCONFIGR_CID;

	 
	if (attr->config & BIT(ETM_OPT_CTXTID2)) {
		if (!is_kernel_in_hyp_mode()) {
			ret = -EINVAL;
			goto out;
		}
		 
		if (task_is_in_init_pid_ns(current))
			config->cfg |= TRCCONFIGR_VMID | TRCCONFIGR_VMIDOPT;
	}

	 
	if ((attr->config & BIT(ETM_OPT_RETSTK)) && drvdata->retstack)
		 
		config->cfg |= TRCCONFIGR_RS;

	 
	if (attr->config2 & GENMASK_ULL(63, 32)) {
		cfg_hash = (u32)(attr->config2 >> 32);
		preset = attr->config & 0xF;
		ret = cscfg_csdev_enable_active_config(csdev, cfg_hash, preset);
	}

	 
	if (attr->config & BIT(ETM_OPT_BRANCH_BROADCAST)) {
		if (!drvdata->trcbb) {
			 
			ret = -EINVAL;
			goto out;
		} else {
			config->cfg |= BIT(ETM4_CFG_BIT_BB);
		}
	}

out:
	return ret;
}

static int etm4_enable_perf(struct coresight_device *csdev,
			    struct perf_event *event)
{
	int ret = 0, trace_id;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	if (WARN_ON_ONCE(drvdata->cpu != smp_processor_id())) {
		ret = -EINVAL;
		goto out;
	}

	 
	ret = etm4_parse_event_config(csdev, event);
	if (ret)
		goto out;

	 
	trace_id = coresight_trace_id_read_cpu_id(drvdata->cpu);
	if (!IS_VALID_CS_TRACE_ID(trace_id)) {
		dev_err(&drvdata->csdev->dev, "Failed to set trace ID for %s on CPU%d\n",
			dev_name(&drvdata->csdev->dev), drvdata->cpu);
		ret = -EINVAL;
		goto out;
	}
	drvdata->trcid = (u8)trace_id;

	 
	ret = etm4_enable_hw(drvdata);

out:
	return ret;
}

static int etm4_enable_sysfs(struct coresight_device *csdev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct etm4_enable_arg arg = { };
	unsigned long cfg_hash;
	int ret, preset;

	 
	cscfg_config_sysfs_get_active_cfg(&cfg_hash, &preset);
	if (cfg_hash) {
		ret = cscfg_csdev_enable_active_config(csdev, cfg_hash, preset);
		if (ret)
			return ret;
	}

	spin_lock(&drvdata->spinlock);

	 
	ret = etm4_read_alloc_trace_id(drvdata);
	if (ret < 0)
		goto unlock_sysfs_enable;

	 
	arg.drvdata = drvdata;
	ret = smp_call_function_single(drvdata->cpu,
				       etm4_enable_hw_smp_call, &arg, 1);
	if (!ret)
		ret = arg.rc;
	if (!ret)
		drvdata->sticky_enable = true;

	if (ret)
		etm4_release_trace_id(drvdata);

unlock_sysfs_enable:
	spin_unlock(&drvdata->spinlock);

	if (!ret)
		dev_dbg(&csdev->dev, "ETM tracing enabled\n");
	return ret;
}

static int etm4_enable(struct coresight_device *csdev, struct perf_event *event,
		       enum cs_mode mode)
{
	int ret;
	u32 val;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	val = local_cmpxchg(&drvdata->mode, CS_MODE_DISABLED, mode);

	 
	if (val)
		return -EBUSY;

	switch (mode) {
	case CS_MODE_SYSFS:
		ret = etm4_enable_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		ret = etm4_enable_perf(csdev, event);
		break;
	default:
		ret = -EINVAL;
	}

	 
	if (ret)
		local_set(&drvdata->mode, CS_MODE_DISABLED);

	return ret;
}

static void etm4_disable_hw(void *info)
{
	u32 control;
	struct etmv4_drvdata *drvdata = info;
	struct etmv4_config *config = &drvdata->config;
	struct coresight_device *csdev = drvdata->csdev;
	struct device *etm_dev = &csdev->dev;
	struct csdev_access *csa = &csdev->access;
	int i;

	etm4_cs_unlock(drvdata, csa);
	etm4_disable_arch_specific(drvdata);

	if (!drvdata->skip_power_up) {
		 
		control = etm4x_relaxed_read32(csa, TRCPDCR);
		control &= ~TRCPDCR_PU;
		etm4x_relaxed_write32(csa, control, TRCPDCR);
	}

	control = etm4x_relaxed_read32(csa, TRCPRGCTLR);

	 
	control &= ~0x1;

	 
	etm4x_prohibit_trace(drvdata);
	 
	dsb(sy);
	isb();
	 
	tsb_csync();
	etm4x_relaxed_write32(csa, control, TRCPRGCTLR);

	 
	if (coresight_timeout(csa, TRCSTATR, TRCSTATR_PMSTABLE_BIT, 1))
		dev_err(etm_dev,
			"timeout while waiting for PM stable Trace Status\n");
	 
	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		config->ss_status[i] =
			etm4x_relaxed_read32(csa, TRCSSCSRn(i));
	}

	 
	for (i = 0; i < drvdata->nr_cntr; i++) {
		config->cntr_val[i] =
			etm4x_relaxed_read32(csa, TRCCNTVRn(i));
	}

	coresight_disclaim_device_unlocked(csdev);
	etm4_cs_lock(drvdata, csa);

	dev_dbg(&drvdata->csdev->dev,
		"cpu: %d disable smp call done\n", drvdata->cpu);
}

static int etm4_disable_perf(struct coresight_device *csdev,
			     struct perf_event *event)
{
	u32 control;
	struct etm_filters *filters = event->hw.addr_filters;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);
	struct perf_event_attr *attr = &event->attr;

	if (WARN_ON_ONCE(drvdata->cpu != smp_processor_id()))
		return -EINVAL;

	etm4_disable_hw(drvdata);
	 
	if (attr->config2 & GENMASK_ULL(63, 32))
		cscfg_csdev_disable_active_config(csdev);

	 
	control = etm4x_relaxed_read32(&csdev->access, TRCVICTLR);
	 
	filters->ssstatus = (control & BIT(9));

	 

	return 0;
}

static void etm4_disable_sysfs(struct coresight_device *csdev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	 
	cpus_read_lock();
	spin_lock(&drvdata->spinlock);

	 
	smp_call_function_single(drvdata->cpu, etm4_disable_hw, drvdata, 1);

	spin_unlock(&drvdata->spinlock);
	cpus_read_unlock();

	 

	dev_dbg(&csdev->dev, "ETM tracing disabled\n");
}

static void etm4_disable(struct coresight_device *csdev,
			 struct perf_event *event)
{
	enum cs_mode mode;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	 
	mode = local_read(&drvdata->mode);

	switch (mode) {
	case CS_MODE_DISABLED:
		break;
	case CS_MODE_SYSFS:
		etm4_disable_sysfs(csdev);
		break;
	case CS_MODE_PERF:
		etm4_disable_perf(csdev, event);
		break;
	}

	if (mode)
		local_set(&drvdata->mode, CS_MODE_DISABLED);
}

static const struct coresight_ops_source etm4_source_ops = {
	.cpu_id		= etm4_cpu_id,
	.enable		= etm4_enable,
	.disable	= etm4_disable,
};

static const struct coresight_ops etm4_cs_ops = {
	.source_ops	= &etm4_source_ops,
};

static inline bool cpu_supports_sysreg_trace(void)
{
	u64 dfr0 = read_sysreg_s(SYS_ID_AA64DFR0_EL1);

	return ((dfr0 >> ID_AA64DFR0_EL1_TraceVer_SHIFT) & 0xfUL) > 0;
}

static bool etm4_init_sysreg_access(struct etmv4_drvdata *drvdata,
				    struct csdev_access *csa)
{
	u32 devarch;

	if (!cpu_supports_sysreg_trace())
		return false;

	 
	devarch = read_etm4x_sysreg_const_offset(TRCDEVARCH);
	switch (devarch & ETM_DEVARCH_ID_MASK) {
	case ETM_DEVARCH_ETMv4x_ARCH:
		*csa = (struct csdev_access) {
			.io_mem	= false,
			.read	= etm4x_sysreg_read,
			.write	= etm4x_sysreg_write,
		};
		break;
	case ETM_DEVARCH_ETE_ARCH:
		*csa = (struct csdev_access) {
			.io_mem	= false,
			.read	= ete_sysreg_read,
			.write	= ete_sysreg_write,
		};
		break;
	default:
		return false;
	}

	drvdata->arch = etm_devarch_to_arch(devarch);
	return true;
}

static bool is_devtype_cpu_trace(void __iomem *base)
{
	u32 devtype = readl(base + TRCDEVTYPE);

	return (devtype == CS_DEVTYPE_PE_TRACE);
}

static bool etm4_init_iomem_access(struct etmv4_drvdata *drvdata,
				   struct csdev_access *csa)
{
	u32 devarch = readl_relaxed(drvdata->base + TRCDEVARCH);

	if (!is_coresight_device(drvdata->base) || !is_devtype_cpu_trace(drvdata->base))
		return false;

	 
	if ((devarch & ETM_DEVARCH_ID_MASK) != ETM_DEVARCH_ETMv4x_ARCH) {
		pr_warn_once("TRCDEVARCH doesn't match ETMv4 architecture\n");
		return false;
	}

	drvdata->arch = etm_devarch_to_arch(devarch);
	*csa = CSDEV_ACCESS_IOMEM(drvdata->base);
	return true;
}

static bool etm4_init_csdev_access(struct etmv4_drvdata *drvdata,
				   struct csdev_access *csa)
{
	 
	if (drvdata->base)
		return etm4_init_iomem_access(drvdata, csa);

	if (etm4_init_sysreg_access(drvdata, csa))
		return true;

	return false;
}

static void cpu_detect_trace_filtering(struct etmv4_drvdata *drvdata)
{
	u64 dfr0 = read_sysreg(id_aa64dfr0_el1);
	u64 trfcr;

	drvdata->trfcr = 0;
	if (!cpuid_feature_extract_unsigned_field(dfr0, ID_AA64DFR0_EL1_TraceFilt_SHIFT))
		return;

	 
	trfcr = (TRFCR_ELx_TS_VIRTUAL |
		 TRFCR_ELx_ExTRE |
		 TRFCR_ELx_E0TRE);

	 
	if (is_kernel_in_hyp_mode())
		trfcr |= TRFCR_EL2_CX;

	drvdata->trfcr = trfcr;
}

static void etm4_init_arch_data(void *info)
{
	u32 etmidr0;
	u32 etmidr2;
	u32 etmidr3;
	u32 etmidr4;
	u32 etmidr5;
	struct etm4_init_arg *init_arg = info;
	struct etmv4_drvdata *drvdata;
	struct csdev_access *csa;
	int i;

	drvdata = dev_get_drvdata(init_arg->dev);
	csa = init_arg->csa;

	 
	if (!etm4_init_csdev_access(drvdata, csa))
		return;

	 
	etm_detect_os_lock(drvdata, csa);

	 
	etm4_os_unlock_csa(drvdata, csa);
	etm4_cs_unlock(drvdata, csa);

	etm4_check_arch_features(drvdata, csa);

	 
	etmidr0 = etm4x_relaxed_read32(csa, TRCIDR0);

	 
	drvdata->instrp0 = !!(FIELD_GET(TRCIDR0_INSTP0_MASK, etmidr0) == 0b11);
	 
	drvdata->trcbb = !!(etmidr0 & TRCIDR0_TRCBB);
	 
	drvdata->trccond = !!(etmidr0 & TRCIDR0_TRCCOND);
	 
	drvdata->trccci = !!(etmidr0 & TRCIDR0_TRCCCI);
	 
	drvdata->retstack = !!(etmidr0 & TRCIDR0_RETSTACK);
	 
	drvdata->nr_event = FIELD_GET(TRCIDR0_NUMEVENT_MASK, etmidr0);
	 
	drvdata->q_support = FIELD_GET(TRCIDR0_QSUPP_MASK, etmidr0);
	 
	drvdata->ts_size = FIELD_GET(TRCIDR0_TSSIZE_MASK, etmidr0);

	 
	etmidr2 = etm4x_relaxed_read32(csa, TRCIDR2);
	 
	drvdata->ctxid_size = FIELD_GET(TRCIDR2_CIDSIZE_MASK, etmidr2);
	 
	drvdata->vmid_size = FIELD_GET(TRCIDR2_VMIDSIZE_MASK, etmidr2);
	 
	drvdata->ccsize = FIELD_GET(TRCIDR2_CCSIZE_MASK, etmidr2);

	etmidr3 = etm4x_relaxed_read32(csa, TRCIDR3);
	 
	drvdata->ccitmin = FIELD_GET(TRCIDR3_CCITMIN_MASK, etmidr3);
	 
	drvdata->s_ex_level = FIELD_GET(TRCIDR3_EXLEVEL_S_MASK, etmidr3);
	drvdata->config.s_ex_level = drvdata->s_ex_level;
	 
	drvdata->ns_ex_level = FIELD_GET(TRCIDR3_EXLEVEL_NS_MASK, etmidr3);
	 
	drvdata->trc_error = !!(etmidr3 & TRCIDR3_TRCERR);
	 
	drvdata->syncpr = !!(etmidr3 & TRCIDR3_SYNCPR);
	 
	drvdata->stallctl = !!(etmidr3 & TRCIDR3_STALLCTL);
	 
	drvdata->sysstall = !!(etmidr3 & TRCIDR3_SYSSTALL);
	 
	drvdata->nr_pe =  (FIELD_GET(TRCIDR3_NUMPROC_HI_MASK, etmidr3) << 3) |
			   FIELD_GET(TRCIDR3_NUMPROC_LO_MASK, etmidr3);
	 
	drvdata->nooverflow = !!(etmidr3 & TRCIDR3_NOOVERFLOW);

	 
	etmidr4 = etm4x_relaxed_read32(csa, TRCIDR4);
	 
	drvdata->nr_addr_cmp = FIELD_GET(TRCIDR4_NUMACPAIRS_MASK, etmidr4);
	 
	drvdata->nr_pe_cmp = FIELD_GET(TRCIDR4_NUMPC_MASK, etmidr4);
	 
	drvdata->nr_resource = FIELD_GET(TRCIDR4_NUMRSPAIR_MASK, etmidr4);
	if ((drvdata->arch < ETM_ARCH_V4_3) || (drvdata->nr_resource > 0))
		drvdata->nr_resource += 1;
	 
	drvdata->nr_ss_cmp = FIELD_GET(TRCIDR4_NUMSSCC_MASK, etmidr4);
	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		drvdata->config.ss_status[i] =
			etm4x_relaxed_read32(csa, TRCSSCSRn(i));
	}
	 
	drvdata->numcidc = FIELD_GET(TRCIDR4_NUMCIDC_MASK, etmidr4);
	 
	drvdata->numvmidc = FIELD_GET(TRCIDR4_NUMVMIDC_MASK, etmidr4);

	etmidr5 = etm4x_relaxed_read32(csa, TRCIDR5);
	 
	drvdata->nr_ext_inp = FIELD_GET(TRCIDR5_NUMEXTIN_MASK, etmidr5);
	 
	drvdata->trcid_size = FIELD_GET(TRCIDR5_TRACEIDSIZE_MASK, etmidr5);
	 
	drvdata->atbtrig = !!(etmidr5 & TRCIDR5_ATBTRIG);
	 
	drvdata->lpoverride = (etmidr5 & TRCIDR5_LPOVERRIDE) && (!drvdata->skip_power_up);
	 
	drvdata->nrseqstate = FIELD_GET(TRCIDR5_NUMSEQSTATE_MASK, etmidr5);
	 
	drvdata->nr_cntr = FIELD_GET(TRCIDR5_NUMCNTR_MASK, etmidr5);
	etm4_cs_lock(drvdata, csa);
	cpu_detect_trace_filtering(drvdata);
}

static inline u32 etm4_get_victlr_access_type(struct etmv4_config *config)
{
	return etm4_get_access_type(config) << __bf_shf(TRCVICTLR_EXLEVEL_MASK);
}

 
static void etm4_set_victlr_access(struct etmv4_config *config)
{
	config->vinst_ctrl &= ~TRCVICTLR_EXLEVEL_MASK;
	config->vinst_ctrl |= etm4_get_victlr_access_type(config);
}

static void etm4_set_default_config(struct etmv4_config *config)
{
	 
	config->eventctrl0 = 0x0;
	config->eventctrl1 = 0x0;

	 
	config->stall_ctrl = 0x0;

	 
	config->syncfreq = 0xC;

	 
	config->ts_ctrl = 0x0;

	 
	config->vinst_ctrl = FIELD_PREP(TRCVICTLR_EVENT_MASK, 0x01);

	 
	etm4_set_victlr_access(config);
}

static u64 etm4_get_ns_access_type(struct etmv4_config *config)
{
	u64 access_type = 0;

	 
	if (!is_kernel_in_hyp_mode()) {
		 
		access_type =  ETM_EXLEVEL_NS_HYP;
		if (config->mode & ETM_MODE_EXCL_KERN)
			access_type |= ETM_EXLEVEL_NS_OS;
	} else if (config->mode & ETM_MODE_EXCL_KERN) {
		access_type = ETM_EXLEVEL_NS_HYP;
	}

	if (config->mode & ETM_MODE_EXCL_USER)
		access_type |= ETM_EXLEVEL_NS_APP;

	return access_type;
}

 
static u64 etm4_get_access_type(struct etmv4_config *config)
{
	 
	return etm4_get_ns_access_type(config) | (u64)config->s_ex_level;
}

static u64 etm4_get_comparator_access_type(struct etmv4_config *config)
{
	return etm4_get_access_type(config) << TRCACATR_EXLEVEL_SHIFT;
}

static void etm4_set_comparator_filter(struct etmv4_config *config,
				       u64 start, u64 stop, int comparator)
{
	u64 access_type = etm4_get_comparator_access_type(config);

	 
	config->addr_val[comparator] = start;
	config->addr_acc[comparator] = access_type;
	config->addr_type[comparator] = ETM_ADDR_TYPE_RANGE;

	 
	config->addr_val[comparator + 1] = stop;
	config->addr_acc[comparator + 1] = access_type;
	config->addr_type[comparator + 1] = ETM_ADDR_TYPE_RANGE;

	 
	config->viiectlr |= BIT(comparator / 2);
}

static void etm4_set_start_stop_filter(struct etmv4_config *config,
				       u64 address, int comparator,
				       enum etm_addr_type type)
{
	int shift;
	u64 access_type = etm4_get_comparator_access_type(config);

	 
	config->addr_val[comparator] = address;
	config->addr_acc[comparator] = access_type;
	config->addr_type[comparator] = type;

	 
	shift = (type == ETM_ADDR_TYPE_START ? 0 : 16);
	config->vissctlr |= BIT(shift + comparator);
}

static void etm4_set_default_filter(struct etmv4_config *config)
{
	 
	config->viiectlr = 0x0;

	 
	config->vinst_ctrl |= TRCVICTLR_SSSTATUS;
	config->mode |= ETM_MODE_VIEWINST_STARTSTOP;

	 
	config->vissctlr = 0x0;
}

static void etm4_set_default(struct etmv4_config *config)
{
	if (WARN_ON_ONCE(!config))
		return;

	 
	etm4_set_default_config(config);
	etm4_set_default_filter(config);
}

static int etm4_get_next_comparator(struct etmv4_drvdata *drvdata, u32 type)
{
	int nr_comparator, index = 0;
	struct etmv4_config *config = &drvdata->config;

	 
	nr_comparator = drvdata->nr_addr_cmp * 2;

	 
	while (index < nr_comparator) {
		switch (type) {
		case ETM_ADDR_TYPE_RANGE:
			if (config->addr_type[index] == ETM_ADDR_TYPE_NONE &&
			    config->addr_type[index + 1] == ETM_ADDR_TYPE_NONE)
				return index;

			 
			index += 2;
			break;
		case ETM_ADDR_TYPE_START:
		case ETM_ADDR_TYPE_STOP:
			if (config->addr_type[index] == ETM_ADDR_TYPE_NONE)
				return index;

			 
			index += 1;
			break;
		default:
			return -EINVAL;
		}
	}

	 
	return -ENOSPC;
}

static int etm4_set_event_filters(struct etmv4_drvdata *drvdata,
				  struct perf_event *event)
{
	int i, comparator, ret = 0;
	u64 address;
	struct etmv4_config *config = &drvdata->config;
	struct etm_filters *filters = event->hw.addr_filters;

	if (!filters)
		goto default_filter;

	 
	perf_event_addr_filters_sync(event);

	 
	if (!filters->nr_filters)
		goto default_filter;

	for (i = 0; i < filters->nr_filters; i++) {
		struct etm_filter *filter = &filters->etm_filter[i];
		enum etm_addr_type type = filter->type;

		 
		comparator = etm4_get_next_comparator(drvdata, type);
		if (comparator < 0) {
			ret = comparator;
			goto out;
		}

		switch (type) {
		case ETM_ADDR_TYPE_RANGE:
			etm4_set_comparator_filter(config,
						   filter->start_addr,
						   filter->stop_addr,
						   comparator);
			 
			config->vinst_ctrl |= TRCVICTLR_SSSTATUS;

			 
			config->vissctlr = 0x0;
			break;
		case ETM_ADDR_TYPE_START:
		case ETM_ADDR_TYPE_STOP:
			 
			address = (type == ETM_ADDR_TYPE_START ?
				   filter->start_addr :
				   filter->stop_addr);

			 
			etm4_set_start_stop_filter(config, address,
						   comparator, type);

			 
			if (filters->ssstatus)
				config->vinst_ctrl |= TRCVICTLR_SSSTATUS;

			 
			config->viiectlr = 0x0;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
	}

	goto out;


default_filter:
	etm4_set_default_filter(config);

out:
	return ret;
}

void etm4_config_trace_mode(struct etmv4_config *config)
{
	u32 mode;

	mode = config->mode;
	mode &= (ETM_MODE_EXCL_KERN | ETM_MODE_EXCL_USER);

	 
	WARN_ON_ONCE(mode == (ETM_MODE_EXCL_KERN | ETM_MODE_EXCL_USER));

	 
	if (!(mode & ETM_MODE_EXCL_KERN) && !(mode & ETM_MODE_EXCL_USER))
		return;

	etm4_set_victlr_access(config);
}

static int etm4_online_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return etm4_probe_cpu(cpu);

	if (etmdrvdata[cpu]->boot_enable && !etmdrvdata[cpu]->sticky_enable)
		coresight_enable(etmdrvdata[cpu]->csdev);
	return 0;
}

static int etm4_starting_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return 0;

	spin_lock(&etmdrvdata[cpu]->spinlock);
	if (!etmdrvdata[cpu]->os_unlock)
		etm4_os_unlock(etmdrvdata[cpu]);

	if (local_read(&etmdrvdata[cpu]->mode))
		etm4_enable_hw(etmdrvdata[cpu]);
	spin_unlock(&etmdrvdata[cpu]->spinlock);
	return 0;
}

static int etm4_dying_cpu(unsigned int cpu)
{
	if (!etmdrvdata[cpu])
		return 0;

	spin_lock(&etmdrvdata[cpu]->spinlock);
	if (local_read(&etmdrvdata[cpu]->mode))
		etm4_disable_hw(etmdrvdata[cpu]);
	spin_unlock(&etmdrvdata[cpu]->spinlock);
	return 0;
}

static int __etm4_cpu_save(struct etmv4_drvdata *drvdata)
{
	int i, ret = 0;
	struct etmv4_save_state *state;
	struct coresight_device *csdev = drvdata->csdev;
	struct csdev_access *csa;
	struct device *etm_dev;

	if (WARN_ON(!csdev))
		return -ENODEV;

	etm_dev = &csdev->dev;
	csa = &csdev->access;

	 
	dsb(sy);
	isb();

	etm4_cs_unlock(drvdata, csa);
	 
	etm4_os_lock(drvdata);

	 
	if (coresight_timeout(csa, TRCSTATR, TRCSTATR_PMSTABLE_BIT, 1)) {
		dev_err(etm_dev,
			"timeout while waiting for PM Stable Status\n");
		etm4_os_unlock(drvdata);
		ret = -EBUSY;
		goto out;
	}

	state = drvdata->save_state;

	state->trcprgctlr = etm4x_read32(csa, TRCPRGCTLR);
	if (drvdata->nr_pe)
		state->trcprocselr = etm4x_read32(csa, TRCPROCSELR);
	state->trcconfigr = etm4x_read32(csa, TRCCONFIGR);
	state->trcauxctlr = etm4x_read32(csa, TRCAUXCTLR);
	state->trceventctl0r = etm4x_read32(csa, TRCEVENTCTL0R);
	state->trceventctl1r = etm4x_read32(csa, TRCEVENTCTL1R);
	if (drvdata->stallctl)
		state->trcstallctlr = etm4x_read32(csa, TRCSTALLCTLR);
	state->trctsctlr = etm4x_read32(csa, TRCTSCTLR);
	state->trcsyncpr = etm4x_read32(csa, TRCSYNCPR);
	state->trcccctlr = etm4x_read32(csa, TRCCCCTLR);
	state->trcbbctlr = etm4x_read32(csa, TRCBBCTLR);
	state->trctraceidr = etm4x_read32(csa, TRCTRACEIDR);
	state->trcqctlr = etm4x_read32(csa, TRCQCTLR);

	state->trcvictlr = etm4x_read32(csa, TRCVICTLR);
	state->trcviiectlr = etm4x_read32(csa, TRCVIIECTLR);
	state->trcvissctlr = etm4x_read32(csa, TRCVISSCTLR);
	if (drvdata->nr_pe_cmp)
		state->trcvipcssctlr = etm4x_read32(csa, TRCVIPCSSCTLR);
	state->trcvdctlr = etm4x_read32(csa, TRCVDCTLR);
	state->trcvdsacctlr = etm4x_read32(csa, TRCVDSACCTLR);
	state->trcvdarcctlr = etm4x_read32(csa, TRCVDARCCTLR);

	for (i = 0; i < drvdata->nrseqstate - 1; i++)
		state->trcseqevr[i] = etm4x_read32(csa, TRCSEQEVRn(i));

	if (drvdata->nrseqstate) {
		state->trcseqrstevr = etm4x_read32(csa, TRCSEQRSTEVR);
		state->trcseqstr = etm4x_read32(csa, TRCSEQSTR);
	}
	state->trcextinselr = etm4x_read32(csa, TRCEXTINSELR);

	for (i = 0; i < drvdata->nr_cntr; i++) {
		state->trccntrldvr[i] = etm4x_read32(csa, TRCCNTRLDVRn(i));
		state->trccntctlr[i] = etm4x_read32(csa, TRCCNTCTLRn(i));
		state->trccntvr[i] = etm4x_read32(csa, TRCCNTVRn(i));
	}

	for (i = 0; i < drvdata->nr_resource * 2; i++)
		state->trcrsctlr[i] = etm4x_read32(csa, TRCRSCTLRn(i));

	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		state->trcssccr[i] = etm4x_read32(csa, TRCSSCCRn(i));
		state->trcsscsr[i] = etm4x_read32(csa, TRCSSCSRn(i));
		if (etm4x_sspcicrn_present(drvdata, i))
			state->trcsspcicr[i] = etm4x_read32(csa, TRCSSPCICRn(i));
	}

	for (i = 0; i < drvdata->nr_addr_cmp * 2; i++) {
		state->trcacvr[i] = etm4x_read64(csa, TRCACVRn(i));
		state->trcacatr[i] = etm4x_read64(csa, TRCACATRn(i));
	}

	 

	for (i = 0; i < drvdata->numcidc; i++)
		state->trccidcvr[i] = etm4x_read64(csa, TRCCIDCVRn(i));

	for (i = 0; i < drvdata->numvmidc; i++)
		state->trcvmidcvr[i] = etm4x_read64(csa, TRCVMIDCVRn(i));

	state->trccidcctlr0 = etm4x_read32(csa, TRCCIDCCTLR0);
	if (drvdata->numcidc > 4)
		state->trccidcctlr1 = etm4x_read32(csa, TRCCIDCCTLR1);

	state->trcvmidcctlr0 = etm4x_read32(csa, TRCVMIDCCTLR0);
	if (drvdata->numvmidc > 4)
		state->trcvmidcctlr0 = etm4x_read32(csa, TRCVMIDCCTLR1);

	state->trcclaimset = etm4x_read32(csa, TRCCLAIMCLR);

	if (!drvdata->skip_power_up)
		state->trcpdcr = etm4x_read32(csa, TRCPDCR);

	 
	if (coresight_timeout(csa, TRCSTATR, TRCSTATR_IDLE_BIT, 1)) {
		dev_err(etm_dev,
			"timeout while waiting for Idle Trace Status\n");
		etm4_os_unlock(drvdata);
		ret = -EBUSY;
		goto out;
	}

	drvdata->state_needs_restore = true;

	 
	if (!drvdata->skip_power_up)
		etm4x_relaxed_write32(csa, (state->trcpdcr & ~TRCPDCR_PU),
				      TRCPDCR);
out:
	etm4_cs_lock(drvdata, csa);
	return ret;
}

static int etm4_cpu_save(struct etmv4_drvdata *drvdata)
{
	int ret = 0;

	 
	if (drvdata->trfcr)
		drvdata->save_trfcr = read_trfcr();
	 
	if (local_read(&drvdata->mode) && drvdata->save_state)
		ret = __etm4_cpu_save(drvdata);
	return ret;
}

static void __etm4_cpu_restore(struct etmv4_drvdata *drvdata)
{
	int i;
	struct etmv4_save_state *state = drvdata->save_state;
	struct csdev_access tmp_csa = CSDEV_ACCESS_IOMEM(drvdata->base);
	struct csdev_access *csa = &tmp_csa;

	etm4_cs_unlock(drvdata, csa);
	etm4x_relaxed_write32(csa, state->trcclaimset, TRCCLAIMSET);

	etm4x_relaxed_write32(csa, state->trcprgctlr, TRCPRGCTLR);
	if (drvdata->nr_pe)
		etm4x_relaxed_write32(csa, state->trcprocselr, TRCPROCSELR);
	etm4x_relaxed_write32(csa, state->trcconfigr, TRCCONFIGR);
	etm4x_relaxed_write32(csa, state->trcauxctlr, TRCAUXCTLR);
	etm4x_relaxed_write32(csa, state->trceventctl0r, TRCEVENTCTL0R);
	etm4x_relaxed_write32(csa, state->trceventctl1r, TRCEVENTCTL1R);
	if (drvdata->stallctl)
		etm4x_relaxed_write32(csa, state->trcstallctlr, TRCSTALLCTLR);
	etm4x_relaxed_write32(csa, state->trctsctlr, TRCTSCTLR);
	etm4x_relaxed_write32(csa, state->trcsyncpr, TRCSYNCPR);
	etm4x_relaxed_write32(csa, state->trcccctlr, TRCCCCTLR);
	etm4x_relaxed_write32(csa, state->trcbbctlr, TRCBBCTLR);
	etm4x_relaxed_write32(csa, state->trctraceidr, TRCTRACEIDR);
	etm4x_relaxed_write32(csa, state->trcqctlr, TRCQCTLR);

	etm4x_relaxed_write32(csa, state->trcvictlr, TRCVICTLR);
	etm4x_relaxed_write32(csa, state->trcviiectlr, TRCVIIECTLR);
	etm4x_relaxed_write32(csa, state->trcvissctlr, TRCVISSCTLR);
	if (drvdata->nr_pe_cmp)
		etm4x_relaxed_write32(csa, state->trcvipcssctlr, TRCVIPCSSCTLR);
	etm4x_relaxed_write32(csa, state->trcvdctlr, TRCVDCTLR);
	etm4x_relaxed_write32(csa, state->trcvdsacctlr, TRCVDSACCTLR);
	etm4x_relaxed_write32(csa, state->trcvdarcctlr, TRCVDARCCTLR);

	for (i = 0; i < drvdata->nrseqstate - 1; i++)
		etm4x_relaxed_write32(csa, state->trcseqevr[i], TRCSEQEVRn(i));

	if (drvdata->nrseqstate) {
		etm4x_relaxed_write32(csa, state->trcseqrstevr, TRCSEQRSTEVR);
		etm4x_relaxed_write32(csa, state->trcseqstr, TRCSEQSTR);
	}
	etm4x_relaxed_write32(csa, state->trcextinselr, TRCEXTINSELR);

	for (i = 0; i < drvdata->nr_cntr; i++) {
		etm4x_relaxed_write32(csa, state->trccntrldvr[i], TRCCNTRLDVRn(i));
		etm4x_relaxed_write32(csa, state->trccntctlr[i], TRCCNTCTLRn(i));
		etm4x_relaxed_write32(csa, state->trccntvr[i], TRCCNTVRn(i));
	}

	for (i = 0; i < drvdata->nr_resource * 2; i++)
		etm4x_relaxed_write32(csa, state->trcrsctlr[i], TRCRSCTLRn(i));

	for (i = 0; i < drvdata->nr_ss_cmp; i++) {
		etm4x_relaxed_write32(csa, state->trcssccr[i], TRCSSCCRn(i));
		etm4x_relaxed_write32(csa, state->trcsscsr[i], TRCSSCSRn(i));
		if (etm4x_sspcicrn_present(drvdata, i))
			etm4x_relaxed_write32(csa, state->trcsspcicr[i], TRCSSPCICRn(i));
	}

	for (i = 0; i < drvdata->nr_addr_cmp * 2; i++) {
		etm4x_relaxed_write64(csa, state->trcacvr[i], TRCACVRn(i));
		etm4x_relaxed_write64(csa, state->trcacatr[i], TRCACATRn(i));
	}

	for (i = 0; i < drvdata->numcidc; i++)
		etm4x_relaxed_write64(csa, state->trccidcvr[i], TRCCIDCVRn(i));

	for (i = 0; i < drvdata->numvmidc; i++)
		etm4x_relaxed_write64(csa, state->trcvmidcvr[i], TRCVMIDCVRn(i));

	etm4x_relaxed_write32(csa, state->trccidcctlr0, TRCCIDCCTLR0);
	if (drvdata->numcidc > 4)
		etm4x_relaxed_write32(csa, state->trccidcctlr1, TRCCIDCCTLR1);

	etm4x_relaxed_write32(csa, state->trcvmidcctlr0, TRCVMIDCCTLR0);
	if (drvdata->numvmidc > 4)
		etm4x_relaxed_write32(csa, state->trcvmidcctlr0, TRCVMIDCCTLR1);

	etm4x_relaxed_write32(csa, state->trcclaimset, TRCCLAIMSET);

	if (!drvdata->skip_power_up)
		etm4x_relaxed_write32(csa, state->trcpdcr, TRCPDCR);

	drvdata->state_needs_restore = false;

	 
	dsb(sy);
	isb();

	 
	etm4_os_unlock(drvdata);
	etm4_cs_lock(drvdata, csa);
}

static void etm4_cpu_restore(struct etmv4_drvdata *drvdata)
{
	if (drvdata->trfcr)
		write_trfcr(drvdata->save_trfcr);
	if (drvdata->state_needs_restore)
		__etm4_cpu_restore(drvdata);
}

static int etm4_cpu_pm_notify(struct notifier_block *nb, unsigned long cmd,
			      void *v)
{
	struct etmv4_drvdata *drvdata;
	unsigned int cpu = smp_processor_id();

	if (!etmdrvdata[cpu])
		return NOTIFY_OK;

	drvdata = etmdrvdata[cpu];

	if (WARN_ON_ONCE(drvdata->cpu != cpu))
		return NOTIFY_BAD;

	switch (cmd) {
	case CPU_PM_ENTER:
		if (etm4_cpu_save(drvdata))
			return NOTIFY_BAD;
		break;
	case CPU_PM_EXIT:
	case CPU_PM_ENTER_FAILED:
		etm4_cpu_restore(drvdata);
		break;
	default:
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static struct notifier_block etm4_cpu_pm_nb = {
	.notifier_call = etm4_cpu_pm_notify,
};

 
static int __init etm4_pm_setup(void)
{
	int ret;

	ret = cpu_pm_register_notifier(&etm4_cpu_pm_nb);
	if (ret)
		return ret;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ARM_CORESIGHT_STARTING,
					"arm/coresight4:starting",
					etm4_starting_cpu, etm4_dying_cpu);

	if (ret)
		goto unregister_notifier;

	ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
					"arm/coresight4:online",
					etm4_online_cpu, NULL);

	 
	if (ret > 0) {
		hp_online = ret;
		return 0;
	}

	 
	cpuhp_remove_state_nocalls(CPUHP_AP_ARM_CORESIGHT_STARTING);

unregister_notifier:
	cpu_pm_unregister_notifier(&etm4_cpu_pm_nb);
	return ret;
}

static void etm4_pm_clear(void)
{
	cpu_pm_unregister_notifier(&etm4_cpu_pm_nb);
	cpuhp_remove_state_nocalls(CPUHP_AP_ARM_CORESIGHT_STARTING);
	if (hp_online) {
		cpuhp_remove_state_nocalls(hp_online);
		hp_online = 0;
	}
}

static int etm4_add_coresight_dev(struct etm4_init_arg *init_arg)
{
	int ret;
	struct coresight_platform_data *pdata = NULL;
	struct device *dev = init_arg->dev;
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev);
	struct coresight_desc desc = { 0 };
	u8 major, minor;
	char *type_name;

	if (!drvdata)
		return -EINVAL;

	desc.access = *init_arg->csa;

	if (!drvdata->arch)
		return -EINVAL;

	 
	if (!desc.access.io_mem ||
	    fwnode_property_present(dev_fwnode(dev), "qcom,skip-power-up"))
		drvdata->skip_power_up = true;

	major = ETM_ARCH_MAJOR_VERSION(drvdata->arch);
	minor = ETM_ARCH_MINOR_VERSION(drvdata->arch);

	if (etm4x_is_ete(drvdata)) {
		type_name = "ete";
		 
		major -= 4;
	} else {
		type_name = "etm";
	}

	desc.name = devm_kasprintf(dev, GFP_KERNEL,
				   "%s%d", type_name, drvdata->cpu);
	if (!desc.name)
		return -ENOMEM;

	etm4_set_default(&drvdata->config);

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	dev->platform_data = pdata;

	desc.type = CORESIGHT_DEV_TYPE_SOURCE;
	desc.subtype.source_subtype = CORESIGHT_DEV_SUBTYPE_SOURCE_PROC;
	desc.ops = &etm4_cs_ops;
	desc.pdata = pdata;
	desc.dev = dev;
	desc.groups = coresight_etmv4_groups;
	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev))
		return PTR_ERR(drvdata->csdev);

	ret = etm_perf_symlink(drvdata->csdev, true);
	if (ret) {
		coresight_unregister(drvdata->csdev);
		return ret;
	}

	 
	ret = etm4_cscfg_register(drvdata->csdev);
	if (ret) {
		coresight_unregister(drvdata->csdev);
		return ret;
	}

	etmdrvdata[drvdata->cpu] = drvdata;

	dev_info(&drvdata->csdev->dev, "CPU%d: %s v%d.%d initialized\n",
		 drvdata->cpu, type_name, major, minor);

	if (boot_enable) {
		coresight_enable(drvdata->csdev);
		drvdata->boot_enable = true;
	}

	return 0;
}

static int etm4_probe(struct device *dev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev);
	struct csdev_access access = { 0 };
	struct etm4_init_arg init_arg = { 0 };
	struct etm4_init_arg *delayed;

	if (WARN_ON(!drvdata))
		return -ENOMEM;

	if (pm_save_enable == PARAM_PM_SAVE_FIRMWARE)
		pm_save_enable = coresight_loses_context_with_cpu(dev) ?
			       PARAM_PM_SAVE_SELF_HOSTED : PARAM_PM_SAVE_NEVER;

	if (pm_save_enable != PARAM_PM_SAVE_NEVER) {
		drvdata->save_state = devm_kmalloc(dev,
				sizeof(struct etmv4_save_state), GFP_KERNEL);
		if (!drvdata->save_state)
			return -ENOMEM;
	}

	spin_lock_init(&drvdata->spinlock);

	drvdata->cpu = coresight_get_cpu(dev);
	if (drvdata->cpu < 0)
		return drvdata->cpu;

	init_arg.dev = dev;
	init_arg.csa = &access;

	 
	cpus_read_lock();
	if (smp_call_function_single(drvdata->cpu,
				etm4_init_arch_data,  &init_arg, 1)) {
		 
		delayed = devm_kmalloc(dev, sizeof(*delayed), GFP_KERNEL);
		if (!delayed) {
			cpus_read_unlock();
			return -ENOMEM;
		}

		*delayed = init_arg;

		per_cpu(delayed_probe, drvdata->cpu) = delayed;

		cpus_read_unlock();
		return 0;
	}
	cpus_read_unlock();

	return etm4_add_coresight_dev(&init_arg);
}

static int etm4_probe_amba(struct amba_device *adev, const struct amba_id *id)
{
	struct etmv4_drvdata *drvdata;
	void __iomem *base;
	struct device *dev = &adev->dev;
	struct resource *res = &adev->res;
	int ret;

	 
	base = devm_ioremap_resource(dev, res);
	if (IS_ERR(base))
		return PTR_ERR(base);

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->base = base;
	dev_set_drvdata(dev, drvdata);
	ret = etm4_probe(dev);
	if (!ret)
		pm_runtime_put(&adev->dev);

	return ret;
}

static int etm4_probe_platform_dev(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	struct etmv4_drvdata *drvdata;
	int ret;

	drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->pclk = coresight_get_enable_apb_pclk(&pdev->dev);
	if (IS_ERR(drvdata->pclk))
		return -ENODEV;

	if (res) {
		drvdata->base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(drvdata->base)) {
			clk_put(drvdata->pclk);
			return PTR_ERR(drvdata->base);
		}
	}

	dev_set_drvdata(&pdev->dev, drvdata);
	pm_runtime_get_noresume(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = etm4_probe(&pdev->dev);

	pm_runtime_put(&pdev->dev);
	return ret;
}

static int etm4_probe_cpu(unsigned int cpu)
{
	int ret;
	struct etm4_init_arg init_arg;
	struct csdev_access access = { 0 };
	struct etm4_init_arg *iap = *this_cpu_ptr(&delayed_probe);

	if (!iap)
		return 0;

	init_arg = *iap;
	devm_kfree(init_arg.dev, iap);
	*this_cpu_ptr(&delayed_probe) = NULL;

	ret = pm_runtime_resume_and_get(init_arg.dev);
	if (ret < 0) {
		dev_err(init_arg.dev, "Failed to get PM runtime!\n");
		return 0;
	}

	init_arg.csa = &access;
	etm4_init_arch_data(&init_arg);

	etm4_add_coresight_dev(&init_arg);

	pm_runtime_put(init_arg.dev);
	return 0;
}

static struct amba_cs_uci_id uci_id_etm4[] = {
	{
		 
		.devarch	= ETM_DEVARCH_ETMv4x_ARCH,
		.devarch_mask	= ETM_DEVARCH_ID_MASK,
		.devtype	= CS_DEVTYPE_PE_TRACE,
	}
};

static void clear_etmdrvdata(void *info)
{
	int cpu = *(int *)info;

	etmdrvdata[cpu] = NULL;
	per_cpu(delayed_probe, cpu) = NULL;
}

static void etm4_remove_dev(struct etmv4_drvdata *drvdata)
{
	bool had_delayed_probe;
	 
	cpus_read_lock();

	had_delayed_probe = per_cpu(delayed_probe, drvdata->cpu);

	 
	if (smp_call_function_single(drvdata->cpu, clear_etmdrvdata, &drvdata->cpu, 1))
		clear_etmdrvdata(&drvdata->cpu);

	cpus_read_unlock();

	if (!had_delayed_probe) {
		etm_perf_symlink(drvdata->csdev, false);
		cscfg_unregister_csdev(drvdata->csdev);
		coresight_unregister(drvdata->csdev);
	}
}

static void etm4_remove_amba(struct amba_device *adev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	if (drvdata)
		etm4_remove_dev(drvdata);
}

static int etm4_remove_platform_dev(struct platform_device *pdev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(&pdev->dev);

	if (drvdata)
		etm4_remove_dev(drvdata);
	pm_runtime_disable(&pdev->dev);

	if (drvdata && !IS_ERR_OR_NULL(drvdata->pclk))
		clk_put(drvdata->pclk);

	return 0;
}

static const struct amba_id etm4_ids[] = {
	CS_AMBA_ID(0x000bb95d),			 
	CS_AMBA_ID(0x000bb95e),			 
	CS_AMBA_ID(0x000bb95a),			 
	CS_AMBA_ID(0x000bb959),			 
	CS_AMBA_UCI_ID(0x000bb9da, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bbd05, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bbd0a, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bbd0c, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bbd41, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000f0205, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000f0211, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bb802, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bb803, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bb805, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bb804, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000bbd0d, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000cc0af, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000b6d01, uci_id_etm4), 
	CS_AMBA_UCI_ID(0x000b6d02, uci_id_etm4), 
	 
	CS_AMBA_MATCH_ALL_UCI(uci_id_etm4),
	{},
};

MODULE_DEVICE_TABLE(amba, etm4_ids);

static struct amba_driver etm4x_amba_driver = {
	.drv = {
		.name   = "coresight-etm4x",
		.owner  = THIS_MODULE,
		.suppress_bind_attrs = true,
	},
	.probe		= etm4_probe_amba,
	.remove         = etm4_remove_amba,
	.id_table	= etm4_ids,
};

#ifdef CONFIG_PM
static int etm4_runtime_suspend(struct device *dev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata->pclk && !IS_ERR(drvdata->pclk))
		clk_disable_unprepare(drvdata->pclk);

	return 0;
}

static int etm4_runtime_resume(struct device *dev)
{
	struct etmv4_drvdata *drvdata = dev_get_drvdata(dev);

	if (drvdata->pclk && !IS_ERR(drvdata->pclk))
		clk_prepare_enable(drvdata->pclk);

	return 0;
}
#endif

static const struct dev_pm_ops etm4_dev_pm_ops = {
	SET_RUNTIME_PM_OPS(etm4_runtime_suspend, etm4_runtime_resume, NULL)
};

static const struct of_device_id etm4_sysreg_match[] = {
	{ .compatible	= "arm,coresight-etm4x-sysreg" },
	{ .compatible	= "arm,embedded-trace-extension" },
	{}
};

#ifdef CONFIG_ACPI
static const struct acpi_device_id etm4x_acpi_ids[] = {
	{"ARMHC500", 0},  
	{}
};
MODULE_DEVICE_TABLE(acpi, etm4x_acpi_ids);
#endif

static struct platform_driver etm4_platform_driver = {
	.probe		= etm4_probe_platform_dev,
	.remove		= etm4_remove_platform_dev,
	.driver			= {
		.name			= "coresight-etm4x",
		.of_match_table		= etm4_sysreg_match,
		.acpi_match_table	= ACPI_PTR(etm4x_acpi_ids),
		.suppress_bind_attrs	= true,
		.pm			= &etm4_dev_pm_ops,
	},
};

static int __init etm4x_init(void)
{
	int ret;

	ret = etm4_pm_setup();

	 
	if (ret)
		return ret;

	ret = amba_driver_register(&etm4x_amba_driver);
	if (ret) {
		pr_err("Error registering etm4x AMBA driver\n");
		goto clear_pm;
	}

	ret = platform_driver_register(&etm4_platform_driver);
	if (!ret)
		return 0;

	pr_err("Error registering etm4x platform driver\n");
	amba_driver_unregister(&etm4x_amba_driver);

clear_pm:
	etm4_pm_clear();
	return ret;
}

static void __exit etm4x_exit(void)
{
	amba_driver_unregister(&etm4x_amba_driver);
	platform_driver_unregister(&etm4_platform_driver);
	etm4_pm_clear();
}

module_init(etm4x_init);
module_exit(etm4x_exit);

MODULE_AUTHOR("Pratik Patel <pratikp@codeaurora.org>");
MODULE_AUTHOR("Mathieu Poirier <mathieu.poirier@linaro.org>");
MODULE_DESCRIPTION("Arm CoreSight Program Flow Trace v4.x driver");
MODULE_LICENSE("GPL v2");
