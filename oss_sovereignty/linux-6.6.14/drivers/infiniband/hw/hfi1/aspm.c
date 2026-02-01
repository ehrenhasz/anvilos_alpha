
 

#include "aspm.h"

 
#define ASPM_TIMER_MS 1000
 
#define ASPM_RESCHED_TIMER_MS (ASPM_TIMER_MS / 2)
 
#define ASPM_TRIGGER_MS 1
#define ASPM_TRIGGER_NS (ASPM_TRIGGER_MS * 1000 * 1000ull)
#define ASPM_L1_SUPPORTED(reg) \
	((((reg) & PCI_EXP_LNKCAP_ASPMS) >> 10) & 0x2)

uint aspm_mode = ASPM_MODE_DISABLED;
module_param_named(aspm, aspm_mode, uint, 0444);
MODULE_PARM_DESC(aspm, "PCIe ASPM: 0: disable, 1: enable, 2: dynamic");

static bool aspm_hw_l1_supported(struct hfi1_devdata *dd)
{
	struct pci_dev *parent = dd->pcidev->bus->self;
	u32 up, dn;

	 
	if (!parent)
		return false;

	pcie_capability_read_dword(dd->pcidev, PCI_EXP_LNKCAP, &dn);
	dn = ASPM_L1_SUPPORTED(dn);

	pcie_capability_read_dword(parent, PCI_EXP_LNKCAP, &up);
	up = ASPM_L1_SUPPORTED(up);

	 
	return (!!dn || is_ax(dd)) && !!up;
}

 
static void aspm_hw_set_l1_ent_latency(struct hfi1_devdata *dd)
{
	u32 l1_ent_lat = 0x4u;
	u32 reg32;

	pci_read_config_dword(dd->pcidev, PCIE_CFG_REG_PL3, &reg32);
	reg32 &= ~PCIE_CFG_REG_PL3_L1_ENT_LATENCY_SMASK;
	reg32 |= l1_ent_lat << PCIE_CFG_REG_PL3_L1_ENT_LATENCY_SHIFT;
	pci_write_config_dword(dd->pcidev, PCIE_CFG_REG_PL3, reg32);
}

static void aspm_hw_enable_l1(struct hfi1_devdata *dd)
{
	struct pci_dev *parent = dd->pcidev->bus->self;

	 
	if (!parent)
		return;

	 
	pcie_capability_clear_and_set_word(parent, PCI_EXP_LNKCTL,
					   PCI_EXP_LNKCTL_ASPMC,
					   PCI_EXP_LNKCTL_ASPM_L1);
	pcie_capability_clear_and_set_word(dd->pcidev, PCI_EXP_LNKCTL,
					   PCI_EXP_LNKCTL_ASPMC,
					   PCI_EXP_LNKCTL_ASPM_L1);
}

void aspm_hw_disable_l1(struct hfi1_devdata *dd)
{
	struct pci_dev *parent = dd->pcidev->bus->self;

	 
	pcie_capability_clear_and_set_word(dd->pcidev, PCI_EXP_LNKCTL,
					   PCI_EXP_LNKCTL_ASPMC, 0x0);
	if (parent)
		pcie_capability_clear_and_set_word(parent, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_ASPMC, 0x0);
}

static  void aspm_enable(struct hfi1_devdata *dd)
{
	if (dd->aspm_enabled || aspm_mode == ASPM_MODE_DISABLED ||
	    !dd->aspm_supported)
		return;

	aspm_hw_enable_l1(dd);
	dd->aspm_enabled = true;
}

static  void aspm_disable(struct hfi1_devdata *dd)
{
	if (!dd->aspm_enabled || aspm_mode == ASPM_MODE_ENABLED)
		return;

	aspm_hw_disable_l1(dd);
	dd->aspm_enabled = false;
}

static  void aspm_disable_inc(struct hfi1_devdata *dd)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->aspm_lock, flags);
	aspm_disable(dd);
	atomic_inc(&dd->aspm_disabled_cnt);
	spin_unlock_irqrestore(&dd->aspm_lock, flags);
}

static  void aspm_enable_dec(struct hfi1_devdata *dd)
{
	unsigned long flags;

	spin_lock_irqsave(&dd->aspm_lock, flags);
	if (atomic_dec_and_test(&dd->aspm_disabled_cnt))
		aspm_enable(dd);
	spin_unlock_irqrestore(&dd->aspm_lock, flags);
}

 
void __aspm_ctx_disable(struct hfi1_ctxtdata *rcd)
{
	bool restart_timer;
	bool close_interrupts;
	unsigned long flags;
	ktime_t now, prev;

	spin_lock_irqsave(&rcd->aspm_lock, flags);
	 
	if (!rcd->aspm_intr_enable)
		goto unlock;

	prev = rcd->aspm_ts_last_intr;
	now = ktime_get();
	rcd->aspm_ts_last_intr = now;

	 
	close_interrupts = ktime_to_ns(ktime_sub(now, prev)) < ASPM_TRIGGER_NS;

	 
	restart_timer = ktime_to_ns(ktime_sub(now, rcd->aspm_ts_timer_sched)) >
				    ASPM_RESCHED_TIMER_MS * NSEC_PER_MSEC;
	restart_timer = restart_timer && close_interrupts;

	 
	if (rcd->aspm_enabled && close_interrupts) {
		aspm_disable_inc(rcd->dd);
		rcd->aspm_enabled = false;
		restart_timer = true;
	}

	if (restart_timer) {
		mod_timer(&rcd->aspm_timer,
			  jiffies + msecs_to_jiffies(ASPM_TIMER_MS));
		rcd->aspm_ts_timer_sched = now;
	}
unlock:
	spin_unlock_irqrestore(&rcd->aspm_lock, flags);
}

 
static  void aspm_ctx_timer_function(struct timer_list *t)
{
	struct hfi1_ctxtdata *rcd = from_timer(rcd, t, aspm_timer);
	unsigned long flags;

	spin_lock_irqsave(&rcd->aspm_lock, flags);
	aspm_enable_dec(rcd->dd);
	rcd->aspm_enabled = true;
	spin_unlock_irqrestore(&rcd->aspm_lock, flags);
}

 
void aspm_disable_all(struct hfi1_devdata *dd)
{
	struct hfi1_ctxtdata *rcd;
	unsigned long flags;
	u16 i;

	for (i = 0; i < dd->first_dyn_alloc_ctxt; i++) {
		rcd = hfi1_rcd_get_by_index(dd, i);
		if (rcd) {
			del_timer_sync(&rcd->aspm_timer);
			spin_lock_irqsave(&rcd->aspm_lock, flags);
			rcd->aspm_intr_enable = false;
			spin_unlock_irqrestore(&rcd->aspm_lock, flags);
			hfi1_rcd_put(rcd);
		}
	}

	aspm_disable(dd);
	atomic_set(&dd->aspm_disabled_cnt, 0);
}

 
void aspm_enable_all(struct hfi1_devdata *dd)
{
	struct hfi1_ctxtdata *rcd;
	unsigned long flags;
	u16 i;

	aspm_enable(dd);

	if (aspm_mode != ASPM_MODE_DYNAMIC)
		return;

	for (i = 0; i < dd->first_dyn_alloc_ctxt; i++) {
		rcd = hfi1_rcd_get_by_index(dd, i);
		if (rcd) {
			spin_lock_irqsave(&rcd->aspm_lock, flags);
			rcd->aspm_intr_enable = true;
			rcd->aspm_enabled = true;
			spin_unlock_irqrestore(&rcd->aspm_lock, flags);
			hfi1_rcd_put(rcd);
		}
	}
}

static  void aspm_ctx_init(struct hfi1_ctxtdata *rcd)
{
	spin_lock_init(&rcd->aspm_lock);
	timer_setup(&rcd->aspm_timer, aspm_ctx_timer_function, 0);
	rcd->aspm_intr_supported = rcd->dd->aspm_supported &&
		aspm_mode == ASPM_MODE_DYNAMIC &&
		rcd->ctxt < rcd->dd->first_dyn_alloc_ctxt;
}

void aspm_init(struct hfi1_devdata *dd)
{
	struct hfi1_ctxtdata *rcd;
	u16 i;

	spin_lock_init(&dd->aspm_lock);
	dd->aspm_supported = aspm_hw_l1_supported(dd);

	for (i = 0; i < dd->first_dyn_alloc_ctxt; i++) {
		rcd = hfi1_rcd_get_by_index(dd, i);
		if (rcd)
			aspm_ctx_init(rcd);
		hfi1_rcd_put(rcd);
	}

	 
	aspm_hw_set_l1_ent_latency(dd);
	dd->aspm_enabled = false;
	aspm_hw_disable_l1(dd);

	 
	aspm_enable_all(dd);
}

void aspm_exit(struct hfi1_devdata *dd)
{
	aspm_disable_all(dd);

	 
	aspm_enable(dd);
}

