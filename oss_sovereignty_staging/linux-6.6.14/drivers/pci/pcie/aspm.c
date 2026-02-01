
 

#include <linux/kernel.h>
#include <linux/math.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/pci.h>
#include <linux/pci_regs.h>
#include <linux/errno.h>
#include <linux/pm.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include "../pci.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "pcie_aspm."

 
#define ASPM_STATE_L0S_UP	(1)	 
#define ASPM_STATE_L0S_DW	(2)	 
#define ASPM_STATE_L1		(4)	 
#define ASPM_STATE_L1_1		(8)	 
#define ASPM_STATE_L1_2		(0x10)	 
#define ASPM_STATE_L1_1_PCIPM	(0x20)	 
#define ASPM_STATE_L1_2_PCIPM	(0x40)	 
#define ASPM_STATE_L1_SS_PCIPM	(ASPM_STATE_L1_1_PCIPM | ASPM_STATE_L1_2_PCIPM)
#define ASPM_STATE_L1_2_MASK	(ASPM_STATE_L1_2 | ASPM_STATE_L1_2_PCIPM)
#define ASPM_STATE_L1SS		(ASPM_STATE_L1_1 | ASPM_STATE_L1_1_PCIPM |\
				 ASPM_STATE_L1_2_MASK)
#define ASPM_STATE_L0S		(ASPM_STATE_L0S_UP | ASPM_STATE_L0S_DW)
#define ASPM_STATE_ALL		(ASPM_STATE_L0S | ASPM_STATE_L1 |	\
				 ASPM_STATE_L1SS)

struct pcie_link_state {
	struct pci_dev *pdev;		 
	struct pci_dev *downstream;	 
	struct pcie_link_state *root;	 
	struct pcie_link_state *parent;	 
	struct list_head sibling;	 

	 
	u32 aspm_support:7;		 
	u32 aspm_enabled:7;		 
	u32 aspm_capable:7;		 
	u32 aspm_default:7;		 
	u32 aspm_disable:7;		 

	 
	u32 clkpm_capable:1;		 
	u32 clkpm_enabled:1;		 
	u32 clkpm_default:1;		 
	u32 clkpm_disable:1;		 
};

static int aspm_disabled, aspm_force;
static bool aspm_support_enabled = true;
static DEFINE_MUTEX(aspm_lock);
static LIST_HEAD(link_list);

#define POLICY_DEFAULT 0	 
#define POLICY_PERFORMANCE 1	 
#define POLICY_POWERSAVE 2	 
#define POLICY_POWER_SUPERSAVE 3  

#ifdef CONFIG_PCIEASPM_PERFORMANCE
static int aspm_policy = POLICY_PERFORMANCE;
#elif defined CONFIG_PCIEASPM_POWERSAVE
static int aspm_policy = POLICY_POWERSAVE;
#elif defined CONFIG_PCIEASPM_POWER_SUPERSAVE
static int aspm_policy = POLICY_POWER_SUPERSAVE;
#else
static int aspm_policy;
#endif

static const char *policy_str[] = {
	[POLICY_DEFAULT] = "default",
	[POLICY_PERFORMANCE] = "performance",
	[POLICY_POWERSAVE] = "powersave",
	[POLICY_POWER_SUPERSAVE] = "powersupersave"
};

 
static struct pci_dev *pci_function_0(struct pci_bus *linkbus)
{
	struct pci_dev *child;

	list_for_each_entry(child, &linkbus->devices, bus_list)
		if (PCI_FUNC(child->devfn) == 0)
			return child;
	return NULL;
}

static int policy_to_aspm_state(struct pcie_link_state *link)
{
	switch (aspm_policy) {
	case POLICY_PERFORMANCE:
		 
		return 0;
	case POLICY_POWERSAVE:
		 
		return (ASPM_STATE_L0S | ASPM_STATE_L1);
	case POLICY_POWER_SUPERSAVE:
		 
		return ASPM_STATE_ALL;
	case POLICY_DEFAULT:
		return link->aspm_default;
	}
	return 0;
}

static int policy_to_clkpm_state(struct pcie_link_state *link)
{
	switch (aspm_policy) {
	case POLICY_PERFORMANCE:
		 
		return 0;
	case POLICY_POWERSAVE:
	case POLICY_POWER_SUPERSAVE:
		 
		return 1;
	case POLICY_DEFAULT:
		return link->clkpm_default;
	}
	return 0;
}

static void pcie_set_clkpm_nocheck(struct pcie_link_state *link, int enable)
{
	struct pci_dev *child;
	struct pci_bus *linkbus = link->pdev->subordinate;
	u32 val = enable ? PCI_EXP_LNKCTL_CLKREQ_EN : 0;

	list_for_each_entry(child, &linkbus->devices, bus_list)
		pcie_capability_clear_and_set_word(child, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_CLKREQ_EN,
						   val);
	link->clkpm_enabled = !!enable;
}

static void pcie_set_clkpm(struct pcie_link_state *link, int enable)
{
	 
	if (!link->clkpm_capable || link->clkpm_disable)
		enable = 0;
	 
	if (link->clkpm_enabled == enable)
		return;
	pcie_set_clkpm_nocheck(link, enable);
}

static void pcie_clkpm_cap_init(struct pcie_link_state *link, int blacklist)
{
	int capable = 1, enabled = 1;
	u32 reg32;
	u16 reg16;
	struct pci_dev *child;
	struct pci_bus *linkbus = link->pdev->subordinate;

	 
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		pcie_capability_read_dword(child, PCI_EXP_LNKCAP, &reg32);
		if (!(reg32 & PCI_EXP_LNKCAP_CLKPM)) {
			capable = 0;
			enabled = 0;
			break;
		}
		pcie_capability_read_word(child, PCI_EXP_LNKCTL, &reg16);
		if (!(reg16 & PCI_EXP_LNKCTL_CLKREQ_EN))
			enabled = 0;
	}
	link->clkpm_enabled = enabled;
	link->clkpm_default = enabled;
	link->clkpm_capable = capable;
	link->clkpm_disable = blacklist ? 1 : 0;
}

 
static void pcie_aspm_configure_common_clock(struct pcie_link_state *link)
{
	int same_clock = 1;
	u16 reg16, ccc, parent_old_ccc, child_old_ccc[8];
	struct pci_dev *child, *parent = link->pdev;
	struct pci_bus *linkbus = parent->subordinate;
	 
	child = list_entry(linkbus->devices.next, struct pci_dev, bus_list);
	BUG_ON(!pci_is_pcie(child));

	 
	pcie_capability_read_word(child, PCI_EXP_LNKSTA, &reg16);
	if (!(reg16 & PCI_EXP_LNKSTA_SLC))
		same_clock = 0;

	 
	pcie_capability_read_word(parent, PCI_EXP_LNKSTA, &reg16);
	if (!(reg16 & PCI_EXP_LNKSTA_SLC))
		same_clock = 0;

	 
	pcie_capability_read_word(parent, PCI_EXP_LNKCTL, &reg16);
	parent_old_ccc = reg16 & PCI_EXP_LNKCTL_CCC;
	if (same_clock && (reg16 & PCI_EXP_LNKCTL_CCC)) {
		bool consistent = true;

		list_for_each_entry(child, &linkbus->devices, bus_list) {
			pcie_capability_read_word(child, PCI_EXP_LNKCTL,
						  &reg16);
			if (!(reg16 & PCI_EXP_LNKCTL_CCC)) {
				consistent = false;
				break;
			}
		}
		if (consistent)
			return;
		pci_info(parent, "ASPM: current common clock configuration is inconsistent, reconfiguring\n");
	}

	ccc = same_clock ? PCI_EXP_LNKCTL_CCC : 0;
	 
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		pcie_capability_read_word(child, PCI_EXP_LNKCTL, &reg16);
		child_old_ccc[PCI_FUNC(child->devfn)] = reg16 & PCI_EXP_LNKCTL_CCC;
		pcie_capability_clear_and_set_word(child, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_CCC, ccc);
	}

	 
	pcie_capability_clear_and_set_word(parent, PCI_EXP_LNKCTL,
					   PCI_EXP_LNKCTL_CCC, ccc);

	if (pcie_retrain_link(link->pdev, true)) {

		 
		pci_err(parent, "ASPM: Could not configure common clock\n");
		list_for_each_entry(child, &linkbus->devices, bus_list)
			pcie_capability_clear_and_set_word(child, PCI_EXP_LNKCTL,
							   PCI_EXP_LNKCTL_CCC,
							   child_old_ccc[PCI_FUNC(child->devfn)]);
		pcie_capability_clear_and_set_word(parent, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_CCC, parent_old_ccc);
	}
}

 
static u32 calc_l0s_latency(u32 lnkcap)
{
	u32 encoding = (lnkcap & PCI_EXP_LNKCAP_L0SEL) >> 12;

	if (encoding == 0x7)
		return (5 * 1000);	 
	return (64 << encoding);
}

 
static u32 calc_l0s_acceptable(u32 encoding)
{
	if (encoding == 0x7)
		return -1U;
	return (64 << encoding);
}

 
static u32 calc_l1_latency(u32 lnkcap)
{
	u32 encoding = (lnkcap & PCI_EXP_LNKCAP_L1EL) >> 15;

	if (encoding == 0x7)
		return (65 * 1000);	 
	return (1000 << encoding);
}

 
static u32 calc_l1_acceptable(u32 encoding)
{
	if (encoding == 0x7)
		return -1U;
	return (1000 << encoding);
}

 
static u32 calc_l12_pwron(struct pci_dev *pdev, u32 scale, u32 val)
{
	switch (scale) {
	case 0:
		return val * 2;
	case 1:
		return val * 10;
	case 2:
		return val * 100;
	}
	pci_err(pdev, "%s: Invalid T_PwrOn scale: %u\n", __func__, scale);
	return 0;
}

 
static void encode_l12_threshold(u32 threshold_us, u32 *scale, u32 *value)
{
	u64 threshold_ns = (u64) threshold_us * 1000;

	 
	if (threshold_ns <= 0x3ff * 1) {
		*scale = 0;		 
		*value = threshold_ns;
	} else if (threshold_ns <= 0x3ff * 32) {
		*scale = 1;		 
		*value = roundup(threshold_ns, 32) / 32;
	} else if (threshold_ns <= 0x3ff * 1024) {
		*scale = 2;		 
		*value = roundup(threshold_ns, 1024) / 1024;
	} else if (threshold_ns <= 0x3ff * 32768) {
		*scale = 3;		 
		*value = roundup(threshold_ns, 32768) / 32768;
	} else if (threshold_ns <= 0x3ff * 1048576) {
		*scale = 4;		 
		*value = roundup(threshold_ns, 1048576) / 1048576;
	} else if (threshold_ns <= 0x3ff * (u64) 33554432) {
		*scale = 5;		 
		*value = roundup(threshold_ns, 33554432) / 33554432;
	} else {
		*scale = 5;
		*value = 0x3ff;		 
	}
}

static void pcie_aspm_check_latency(struct pci_dev *endpoint)
{
	u32 latency, encoding, lnkcap_up, lnkcap_dw;
	u32 l1_switch_latency = 0, latency_up_l0s;
	u32 latency_up_l1, latency_dw_l0s, latency_dw_l1;
	u32 acceptable_l0s, acceptable_l1;
	struct pcie_link_state *link;

	 
	if ((endpoint->current_state != PCI_D0) &&
	    (endpoint->current_state != PCI_UNKNOWN))
		return;

	link = endpoint->bus->self->link_state;

	 
	encoding = (endpoint->devcap & PCI_EXP_DEVCAP_L0S) >> 6;
	acceptable_l0s = calc_l0s_acceptable(encoding);

	 
	encoding = (endpoint->devcap & PCI_EXP_DEVCAP_L1) >> 9;
	acceptable_l1 = calc_l1_acceptable(encoding);

	while (link) {
		struct pci_dev *dev = pci_function_0(link->pdev->subordinate);

		 
		pcie_capability_read_dword(link->pdev, PCI_EXP_LNKCAP,
					   &lnkcap_up);
		pcie_capability_read_dword(dev, PCI_EXP_LNKCAP,
					   &lnkcap_dw);
		latency_up_l0s = calc_l0s_latency(lnkcap_up);
		latency_up_l1 = calc_l1_latency(lnkcap_up);
		latency_dw_l0s = calc_l0s_latency(lnkcap_dw);
		latency_dw_l1 = calc_l1_latency(lnkcap_dw);

		 
		if ((link->aspm_capable & ASPM_STATE_L0S_UP) &&
		    (latency_up_l0s > acceptable_l0s))
			link->aspm_capable &= ~ASPM_STATE_L0S_UP;

		 
		if ((link->aspm_capable & ASPM_STATE_L0S_DW) &&
		    (latency_dw_l0s > acceptable_l0s))
			link->aspm_capable &= ~ASPM_STATE_L0S_DW;
		 
		latency = max_t(u32, latency_up_l1, latency_dw_l1);
		if ((link->aspm_capable & ASPM_STATE_L1) &&
		    (latency + l1_switch_latency > acceptable_l1))
			link->aspm_capable &= ~ASPM_STATE_L1;
		l1_switch_latency += 1000;

		link = link->parent;
	}
}

static void pci_clear_and_set_dword(struct pci_dev *pdev, int pos,
				    u32 clear, u32 set)
{
	u32 val;

	pci_read_config_dword(pdev, pos, &val);
	val &= ~clear;
	val |= set;
	pci_write_config_dword(pdev, pos, val);
}

 
static void aspm_calc_l12_info(struct pcie_link_state *link,
				u32 parent_l1ss_cap, u32 child_l1ss_cap)
{
	struct pci_dev *child = link->downstream, *parent = link->pdev;
	u32 val1, val2, scale1, scale2;
	u32 t_common_mode, t_power_on, l1_2_threshold, scale, value;
	u32 ctl1 = 0, ctl2 = 0;
	u32 pctl1, pctl2, cctl1, cctl2;
	u32 pl1_2_enables, cl1_2_enables;

	 
	val1 = (parent_l1ss_cap & PCI_L1SS_CAP_CM_RESTORE_TIME) >> 8;
	val2 = (child_l1ss_cap & PCI_L1SS_CAP_CM_RESTORE_TIME) >> 8;
	t_common_mode = max(val1, val2);

	 
	val1   = (parent_l1ss_cap & PCI_L1SS_CAP_P_PWR_ON_VALUE) >> 19;
	scale1 = (parent_l1ss_cap & PCI_L1SS_CAP_P_PWR_ON_SCALE) >> 16;
	val2   = (child_l1ss_cap & PCI_L1SS_CAP_P_PWR_ON_VALUE) >> 19;
	scale2 = (child_l1ss_cap & PCI_L1SS_CAP_P_PWR_ON_SCALE) >> 16;

	if (calc_l12_pwron(parent, scale1, val1) >
	    calc_l12_pwron(child, scale2, val2)) {
		ctl2 |= scale1 | (val1 << 3);
		t_power_on = calc_l12_pwron(parent, scale1, val1);
	} else {
		ctl2 |= scale2 | (val2 << 3);
		t_power_on = calc_l12_pwron(child, scale2, val2);
	}

	 
	l1_2_threshold = 2 + 4 + t_common_mode + t_power_on;
	encode_l12_threshold(l1_2_threshold, &scale, &value);
	ctl1 |= t_common_mode << 8 | scale << 29 | value << 16;

	 
	pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CTL1, &pctl1);
	pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CTL2, &pctl2);
	pci_read_config_dword(child, child->l1ss + PCI_L1SS_CTL1, &cctl1);
	pci_read_config_dword(child, child->l1ss + PCI_L1SS_CTL2, &cctl2);

	if (ctl1 == pctl1 && ctl1 == cctl1 &&
	    ctl2 == pctl2 && ctl2 == cctl2)
		return;

	 
	pl1_2_enables = pctl1 & PCI_L1SS_CTL1_L1_2_MASK;
	cl1_2_enables = cctl1 & PCI_L1SS_CTL1_L1_2_MASK;

	if (pl1_2_enables || cl1_2_enables) {
		pci_clear_and_set_dword(child, child->l1ss + PCI_L1SS_CTL1,
					PCI_L1SS_CTL1_L1_2_MASK, 0);
		pci_clear_and_set_dword(parent, parent->l1ss + PCI_L1SS_CTL1,
					PCI_L1SS_CTL1_L1_2_MASK, 0);
	}

	 
	pci_write_config_dword(parent, parent->l1ss + PCI_L1SS_CTL2, ctl2);
	pci_write_config_dword(child, child->l1ss + PCI_L1SS_CTL2, ctl2);

	 
	pci_clear_and_set_dword(parent, parent->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_CM_RESTORE_TIME, ctl1);

	 
	pci_clear_and_set_dword(parent,	parent->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_LTR_L12_TH_VALUE |
				PCI_L1SS_CTL1_LTR_L12_TH_SCALE, ctl1);
	pci_clear_and_set_dword(child, child->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_LTR_L12_TH_VALUE |
				PCI_L1SS_CTL1_LTR_L12_TH_SCALE, ctl1);

	if (pl1_2_enables || cl1_2_enables) {
		pci_clear_and_set_dword(parent, parent->l1ss + PCI_L1SS_CTL1, 0,
					pl1_2_enables);
		pci_clear_and_set_dword(child, child->l1ss + PCI_L1SS_CTL1, 0,
					cl1_2_enables);
	}
}

static void aspm_l1ss_init(struct pcie_link_state *link)
{
	struct pci_dev *child = link->downstream, *parent = link->pdev;
	u32 parent_l1ss_cap, child_l1ss_cap;
	u32 parent_l1ss_ctl1 = 0, child_l1ss_ctl1 = 0;

	if (!parent->l1ss || !child->l1ss)
		return;

	 
	pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CAP,
			      &parent_l1ss_cap);
	pci_read_config_dword(child, child->l1ss + PCI_L1SS_CAP,
			      &child_l1ss_cap);

	if (!(parent_l1ss_cap & PCI_L1SS_CAP_L1_PM_SS))
		parent_l1ss_cap = 0;
	if (!(child_l1ss_cap & PCI_L1SS_CAP_L1_PM_SS))
		child_l1ss_cap = 0;

	 
	if (!child->ltr_path)
		child_l1ss_cap &= ~PCI_L1SS_CAP_ASPM_L1_2;

	if (parent_l1ss_cap & child_l1ss_cap & PCI_L1SS_CAP_ASPM_L1_1)
		link->aspm_support |= ASPM_STATE_L1_1;
	if (parent_l1ss_cap & child_l1ss_cap & PCI_L1SS_CAP_ASPM_L1_2)
		link->aspm_support |= ASPM_STATE_L1_2;
	if (parent_l1ss_cap & child_l1ss_cap & PCI_L1SS_CAP_PCIPM_L1_1)
		link->aspm_support |= ASPM_STATE_L1_1_PCIPM;
	if (parent_l1ss_cap & child_l1ss_cap & PCI_L1SS_CAP_PCIPM_L1_2)
		link->aspm_support |= ASPM_STATE_L1_2_PCIPM;

	if (parent_l1ss_cap)
		pci_read_config_dword(parent, parent->l1ss + PCI_L1SS_CTL1,
				      &parent_l1ss_ctl1);
	if (child_l1ss_cap)
		pci_read_config_dword(child, child->l1ss + PCI_L1SS_CTL1,
				      &child_l1ss_ctl1);

	if (parent_l1ss_ctl1 & child_l1ss_ctl1 & PCI_L1SS_CTL1_ASPM_L1_1)
		link->aspm_enabled |= ASPM_STATE_L1_1;
	if (parent_l1ss_ctl1 & child_l1ss_ctl1 & PCI_L1SS_CTL1_ASPM_L1_2)
		link->aspm_enabled |= ASPM_STATE_L1_2;
	if (parent_l1ss_ctl1 & child_l1ss_ctl1 & PCI_L1SS_CTL1_PCIPM_L1_1)
		link->aspm_enabled |= ASPM_STATE_L1_1_PCIPM;
	if (parent_l1ss_ctl1 & child_l1ss_ctl1 & PCI_L1SS_CTL1_PCIPM_L1_2)
		link->aspm_enabled |= ASPM_STATE_L1_2_PCIPM;

	if (link->aspm_support & ASPM_STATE_L1_2_MASK)
		aspm_calc_l12_info(link, parent_l1ss_cap, child_l1ss_cap);
}

static void pcie_aspm_cap_init(struct pcie_link_state *link, int blacklist)
{
	struct pci_dev *child = link->downstream, *parent = link->pdev;
	u32 parent_lnkcap, child_lnkcap;
	u16 parent_lnkctl, child_lnkctl;
	struct pci_bus *linkbus = parent->subordinate;

	if (blacklist) {
		 
		link->aspm_enabled = ASPM_STATE_ALL;
		link->aspm_disable = ASPM_STATE_ALL;
		return;
	}

	 
	pcie_capability_read_dword(parent, PCI_EXP_LNKCAP, &parent_lnkcap);
	pcie_capability_read_dword(child, PCI_EXP_LNKCAP, &child_lnkcap);
	if (!(parent_lnkcap & child_lnkcap & PCI_EXP_LNKCAP_ASPMS))
		return;

	 
	pcie_aspm_configure_common_clock(link);

	 
	pcie_capability_read_dword(parent, PCI_EXP_LNKCAP, &parent_lnkcap);
	pcie_capability_read_dword(child, PCI_EXP_LNKCAP, &child_lnkcap);
	pcie_capability_read_word(parent, PCI_EXP_LNKCTL, &parent_lnkctl);
	pcie_capability_read_word(child, PCI_EXP_LNKCTL, &child_lnkctl);

	 
	if (parent_lnkcap & child_lnkcap & PCI_EXP_LNKCAP_ASPM_L0S)
		link->aspm_support |= ASPM_STATE_L0S;

	if (child_lnkctl & PCI_EXP_LNKCTL_ASPM_L0S)
		link->aspm_enabled |= ASPM_STATE_L0S_UP;
	if (parent_lnkctl & PCI_EXP_LNKCTL_ASPM_L0S)
		link->aspm_enabled |= ASPM_STATE_L0S_DW;

	 
	if (parent_lnkcap & child_lnkcap & PCI_EXP_LNKCAP_ASPM_L1)
		link->aspm_support |= ASPM_STATE_L1;

	if (parent_lnkctl & child_lnkctl & PCI_EXP_LNKCTL_ASPM_L1)
		link->aspm_enabled |= ASPM_STATE_L1;

	aspm_l1ss_init(link);

	 
	link->aspm_default = link->aspm_enabled;

	 
	link->aspm_capable = link->aspm_support;

	 
	list_for_each_entry(child, &linkbus->devices, bus_list) {
		if (pci_pcie_type(child) != PCI_EXP_TYPE_ENDPOINT &&
		    pci_pcie_type(child) != PCI_EXP_TYPE_LEG_END)
			continue;

		pcie_aspm_check_latency(child);
	}
}

 
static void pcie_config_aspm_l1ss(struct pcie_link_state *link, u32 state)
{
	u32 val, enable_req;
	struct pci_dev *child = link->downstream, *parent = link->pdev;

	enable_req = (link->aspm_enabled ^ state) & state;

	 

	 
	pci_clear_and_set_dword(child, child->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_L1SS_MASK, 0);
	pci_clear_and_set_dword(parent, parent->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_L1SS_MASK, 0);
	 
	if (enable_req & (ASPM_STATE_L1_1 | ASPM_STATE_L1_2)) {
		pcie_capability_clear_and_set_word(child, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_ASPM_L1, 0);
		pcie_capability_clear_and_set_word(parent, PCI_EXP_LNKCTL,
						   PCI_EXP_LNKCTL_ASPM_L1, 0);
	}

	val = 0;
	if (state & ASPM_STATE_L1_1)
		val |= PCI_L1SS_CTL1_ASPM_L1_1;
	if (state & ASPM_STATE_L1_2)
		val |= PCI_L1SS_CTL1_ASPM_L1_2;
	if (state & ASPM_STATE_L1_1_PCIPM)
		val |= PCI_L1SS_CTL1_PCIPM_L1_1;
	if (state & ASPM_STATE_L1_2_PCIPM)
		val |= PCI_L1SS_CTL1_PCIPM_L1_2;

	 
	pci_clear_and_set_dword(parent, parent->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_L1SS_MASK, val);
	pci_clear_and_set_dword(child, child->l1ss + PCI_L1SS_CTL1,
				PCI_L1SS_CTL1_L1SS_MASK, val);
}

static void pcie_config_aspm_dev(struct pci_dev *pdev, u32 val)
{
	pcie_capability_clear_and_set_word(pdev, PCI_EXP_LNKCTL,
					   PCI_EXP_LNKCTL_ASPMC, val);
}

static void pcie_config_aspm_link(struct pcie_link_state *link, u32 state)
{
	u32 upstream = 0, dwstream = 0;
	struct pci_dev *child = link->downstream, *parent = link->pdev;
	struct pci_bus *linkbus = parent->subordinate;

	 
	state &= (link->aspm_capable & ~link->aspm_disable);

	 
	if (!(state & ASPM_STATE_L1))
		state &= ~ASPM_STATE_L1SS;

	 
	if (parent->current_state != PCI_D0 || child->current_state != PCI_D0) {
		state &= ~ASPM_STATE_L1_SS_PCIPM;
		state |= (link->aspm_enabled & ASPM_STATE_L1_SS_PCIPM);
	}

	 
	if (link->aspm_enabled == state)
		return;
	 
	if (state & ASPM_STATE_L0S_UP)
		dwstream |= PCI_EXP_LNKCTL_ASPM_L0S;
	if (state & ASPM_STATE_L0S_DW)
		upstream |= PCI_EXP_LNKCTL_ASPM_L0S;
	if (state & ASPM_STATE_L1) {
		upstream |= PCI_EXP_LNKCTL_ASPM_L1;
		dwstream |= PCI_EXP_LNKCTL_ASPM_L1;
	}

	if (link->aspm_capable & ASPM_STATE_L1SS)
		pcie_config_aspm_l1ss(link, state);

	 
	if (state & ASPM_STATE_L1)
		pcie_config_aspm_dev(parent, upstream);
	list_for_each_entry(child, &linkbus->devices, bus_list)
		pcie_config_aspm_dev(child, dwstream);
	if (!(state & ASPM_STATE_L1))
		pcie_config_aspm_dev(parent, upstream);

	link->aspm_enabled = state;
}

static void pcie_config_aspm_path(struct pcie_link_state *link)
{
	while (link) {
		pcie_config_aspm_link(link, policy_to_aspm_state(link));
		link = link->parent;
	}
}

static void free_link_state(struct pcie_link_state *link)
{
	link->pdev->link_state = NULL;
	kfree(link);
}

static int pcie_aspm_sanity_check(struct pci_dev *pdev)
{
	struct pci_dev *child;
	u32 reg32;

	 
	list_for_each_entry(child, &pdev->subordinate->devices, bus_list) {
		if (!pci_is_pcie(child))
			return -EINVAL;

		 

		if (aspm_disabled)
			continue;

		 
		pcie_capability_read_dword(child, PCI_EXP_DEVCAP, &reg32);
		if (!(reg32 & PCI_EXP_DEVCAP_RBER) && !aspm_force) {
			pci_info(child, "disabling ASPM on pre-1.1 PCIe device.  You can enable it with 'pcie_aspm=force'\n");
			return -EINVAL;
		}
	}
	return 0;
}

static struct pcie_link_state *alloc_pcie_link_state(struct pci_dev *pdev)
{
	struct pcie_link_state *link;

	link = kzalloc(sizeof(*link), GFP_KERNEL);
	if (!link)
		return NULL;

	INIT_LIST_HEAD(&link->sibling);
	link->pdev = pdev;
	link->downstream = pci_function_0(pdev->subordinate);

	 
	if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT ||
	    pci_pcie_type(pdev) == PCI_EXP_TYPE_PCIE_BRIDGE ||
	    !pdev->bus->parent->self) {
		link->root = link;
	} else {
		struct pcie_link_state *parent;

		parent = pdev->bus->parent->self->link_state;
		if (!parent) {
			kfree(link);
			return NULL;
		}

		link->parent = parent;
		link->root = link->parent->root;
	}

	list_add(&link->sibling, &link_list);
	pdev->link_state = link;
	return link;
}

static void pcie_aspm_update_sysfs_visibility(struct pci_dev *pdev)
{
	struct pci_dev *child;

	list_for_each_entry(child, &pdev->subordinate->devices, bus_list)
		sysfs_update_group(&child->dev.kobj, &aspm_ctrl_attr_group);
}

 
void pcie_aspm_init_link_state(struct pci_dev *pdev)
{
	struct pcie_link_state *link;
	int blacklist = !!pcie_aspm_sanity_check(pdev);

	if (!aspm_support_enabled)
		return;

	if (pdev->link_state)
		return;

	 
	if (!pcie_downstream_port(pdev))
		return;

	 
	if (pci_pcie_type(pdev) == PCI_EXP_TYPE_ROOT_PORT &&
	    pdev->bus->self)
		return;

	down_read(&pci_bus_sem);
	if (list_empty(&pdev->subordinate->devices))
		goto out;

	mutex_lock(&aspm_lock);
	link = alloc_pcie_link_state(pdev);
	if (!link)
		goto unlock;
	 
	pcie_aspm_cap_init(link, blacklist);

	 
	pcie_clkpm_cap_init(link, blacklist);

	 
	if (aspm_policy != POLICY_POWERSAVE &&
	    aspm_policy != POLICY_POWER_SUPERSAVE) {
		pcie_config_aspm_path(link);
		pcie_set_clkpm(link, policy_to_clkpm_state(link));
	}

	pcie_aspm_update_sysfs_visibility(pdev);

unlock:
	mutex_unlock(&aspm_lock);
out:
	up_read(&pci_bus_sem);
}

 
static void pcie_update_aspm_capable(struct pcie_link_state *root)
{
	struct pcie_link_state *link;
	BUG_ON(root->parent);
	list_for_each_entry(link, &link_list, sibling) {
		if (link->root != root)
			continue;
		link->aspm_capable = link->aspm_support;
	}
	list_for_each_entry(link, &link_list, sibling) {
		struct pci_dev *child;
		struct pci_bus *linkbus = link->pdev->subordinate;
		if (link->root != root)
			continue;
		list_for_each_entry(child, &linkbus->devices, bus_list) {
			if ((pci_pcie_type(child) != PCI_EXP_TYPE_ENDPOINT) &&
			    (pci_pcie_type(child) != PCI_EXP_TYPE_LEG_END))
				continue;
			pcie_aspm_check_latency(child);
		}
	}
}

 
void pcie_aspm_exit_link_state(struct pci_dev *pdev)
{
	struct pci_dev *parent = pdev->bus->self;
	struct pcie_link_state *link, *root, *parent_link;

	if (!parent || !parent->link_state)
		return;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);

	link = parent->link_state;
	root = link->root;
	parent_link = link->parent;

	 
	pcie_config_aspm_link(link, 0);
	list_del(&link->sibling);
	free_link_state(link);

	 
	if (parent_link) {
		pcie_update_aspm_capable(root);
		pcie_config_aspm_path(parent_link);
	}

	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

 
void pcie_aspm_pm_state_change(struct pci_dev *pdev)
{
	struct pcie_link_state *link = pdev->link_state;

	if (aspm_disabled || !link)
		return;
	 
	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	pcie_update_aspm_capable(link->root);
	pcie_config_aspm_path(link);
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

void pcie_aspm_powersave_config_link(struct pci_dev *pdev)
{
	struct pcie_link_state *link = pdev->link_state;

	if (aspm_disabled || !link)
		return;

	if (aspm_policy != POLICY_POWERSAVE &&
	    aspm_policy != POLICY_POWER_SUPERSAVE)
		return;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	pcie_config_aspm_path(link);
	pcie_set_clkpm(link, policy_to_clkpm_state(link));
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
}

static struct pcie_link_state *pcie_aspm_get_link(struct pci_dev *pdev)
{
	struct pci_dev *bridge;

	if (!pci_is_pcie(pdev))
		return NULL;

	bridge = pci_upstream_bridge(pdev);
	if (!bridge || !pci_is_pcie(bridge))
		return NULL;

	return bridge->link_state;
}

static int __pci_disable_link_state(struct pci_dev *pdev, int state, bool sem)
{
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);

	if (!link)
		return -EINVAL;
	 
	if (aspm_disabled) {
		pci_warn(pdev, "can't disable ASPM; OS doesn't have ASPM control\n");
		return -EPERM;
	}

	if (sem)
		down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	if (state & PCIE_LINK_STATE_L0S)
		link->aspm_disable |= ASPM_STATE_L0S;
	if (state & PCIE_LINK_STATE_L1)
		 
		link->aspm_disable |= ASPM_STATE_L1 | ASPM_STATE_L1SS;
	if (state & PCIE_LINK_STATE_L1_1)
		link->aspm_disable |= ASPM_STATE_L1_1;
	if (state & PCIE_LINK_STATE_L1_2)
		link->aspm_disable |= ASPM_STATE_L1_2;
	if (state & PCIE_LINK_STATE_L1_1_PCIPM)
		link->aspm_disable |= ASPM_STATE_L1_1_PCIPM;
	if (state & PCIE_LINK_STATE_L1_2_PCIPM)
		link->aspm_disable |= ASPM_STATE_L1_2_PCIPM;
	pcie_config_aspm_link(link, policy_to_aspm_state(link));

	if (state & PCIE_LINK_STATE_CLKPM)
		link->clkpm_disable = 1;
	pcie_set_clkpm(link, policy_to_clkpm_state(link));
	mutex_unlock(&aspm_lock);
	if (sem)
		up_read(&pci_bus_sem);

	return 0;
}

int pci_disable_link_state_locked(struct pci_dev *pdev, int state)
{
	return __pci_disable_link_state(pdev, state, false);
}
EXPORT_SYMBOL(pci_disable_link_state_locked);

 
int pci_disable_link_state(struct pci_dev *pdev, int state)
{
	return __pci_disable_link_state(pdev, state, true);
}
EXPORT_SYMBOL(pci_disable_link_state);

static int __pci_enable_link_state(struct pci_dev *pdev, int state, bool locked)
{
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);

	if (!link)
		return -EINVAL;
	 
	if (aspm_disabled) {
		pci_warn(pdev, "can't override BIOS ASPM; OS doesn't have ASPM control\n");
		return -EPERM;
	}

	if (!locked)
		down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	link->aspm_default = 0;
	if (state & PCIE_LINK_STATE_L0S)
		link->aspm_default |= ASPM_STATE_L0S;
	if (state & PCIE_LINK_STATE_L1)
		link->aspm_default |= ASPM_STATE_L1;
	 
	if (state & PCIE_LINK_STATE_L1_1)
		link->aspm_default |= ASPM_STATE_L1_1 | ASPM_STATE_L1;
	if (state & PCIE_LINK_STATE_L1_2)
		link->aspm_default |= ASPM_STATE_L1_2 | ASPM_STATE_L1;
	if (state & PCIE_LINK_STATE_L1_1_PCIPM)
		link->aspm_default |= ASPM_STATE_L1_1_PCIPM | ASPM_STATE_L1;
	if (state & PCIE_LINK_STATE_L1_2_PCIPM)
		link->aspm_default |= ASPM_STATE_L1_2_PCIPM | ASPM_STATE_L1;
	pcie_config_aspm_link(link, policy_to_aspm_state(link));

	link->clkpm_default = (state & PCIE_LINK_STATE_CLKPM) ? 1 : 0;
	pcie_set_clkpm(link, policy_to_clkpm_state(link));
	mutex_unlock(&aspm_lock);
	if (!locked)
		up_read(&pci_bus_sem);

	return 0;
}

 
int pci_enable_link_state(struct pci_dev *pdev, int state)
{
	return __pci_enable_link_state(pdev, state, false);
}
EXPORT_SYMBOL(pci_enable_link_state);

 
int pci_enable_link_state_locked(struct pci_dev *pdev, int state)
{
	lockdep_assert_held_read(&pci_bus_sem);

	return __pci_enable_link_state(pdev, state, true);
}
EXPORT_SYMBOL(pci_enable_link_state_locked);

static int pcie_aspm_set_policy(const char *val,
				const struct kernel_param *kp)
{
	int i;
	struct pcie_link_state *link;

	if (aspm_disabled)
		return -EPERM;
	i = sysfs_match_string(policy_str, val);
	if (i < 0)
		return i;
	if (i == aspm_policy)
		return 0;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);
	aspm_policy = i;
	list_for_each_entry(link, &link_list, sibling) {
		pcie_config_aspm_link(link, policy_to_aspm_state(link));
		pcie_set_clkpm(link, policy_to_clkpm_state(link));
	}
	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);
	return 0;
}

static int pcie_aspm_get_policy(char *buffer, const struct kernel_param *kp)
{
	int i, cnt = 0;
	for (i = 0; i < ARRAY_SIZE(policy_str); i++)
		if (i == aspm_policy)
			cnt += sprintf(buffer + cnt, "[%s] ", policy_str[i]);
		else
			cnt += sprintf(buffer + cnt, "%s ", policy_str[i]);
	cnt += sprintf(buffer + cnt, "\n");
	return cnt;
}

module_param_call(policy, pcie_aspm_set_policy, pcie_aspm_get_policy,
	NULL, 0644);

 
bool pcie_aspm_enabled(struct pci_dev *pdev)
{
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);

	if (!link)
		return false;

	return link->aspm_enabled;
}
EXPORT_SYMBOL_GPL(pcie_aspm_enabled);

static ssize_t aspm_attr_show_common(struct device *dev,
				     struct device_attribute *attr,
				     char *buf, u8 state)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);

	return sysfs_emit(buf, "%d\n", (link->aspm_enabled & state) ? 1 : 0);
}

static ssize_t aspm_attr_store_common(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t len, u8 state)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);
	bool state_enable;

	if (kstrtobool(buf, &state_enable) < 0)
		return -EINVAL;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);

	if (state_enable) {
		link->aspm_disable &= ~state;
		 
		if (state & ASPM_STATE_L1SS)
			link->aspm_disable &= ~ASPM_STATE_L1;
	} else {
		link->aspm_disable |= state;
		if (state & ASPM_STATE_L1)
			link->aspm_disable |= ASPM_STATE_L1SS;
	}

	pcie_config_aspm_link(link, policy_to_aspm_state(link));

	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);

	return len;
}

#define ASPM_ATTR(_f, _s)						\
static ssize_t _f##_show(struct device *dev,				\
			 struct device_attribute *attr, char *buf)	\
{ return aspm_attr_show_common(dev, attr, buf, ASPM_STATE_##_s); }	\
									\
static ssize_t _f##_store(struct device *dev,				\
			  struct device_attribute *attr,		\
			  const char *buf, size_t len)			\
{ return aspm_attr_store_common(dev, attr, buf, len, ASPM_STATE_##_s); }

ASPM_ATTR(l0s_aspm, L0S)
ASPM_ATTR(l1_aspm, L1)
ASPM_ATTR(l1_1_aspm, L1_1)
ASPM_ATTR(l1_2_aspm, L1_2)
ASPM_ATTR(l1_1_pcipm, L1_1_PCIPM)
ASPM_ATTR(l1_2_pcipm, L1_2_PCIPM)

static ssize_t clkpm_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);

	return sysfs_emit(buf, "%d\n", link->clkpm_enabled);
}

static ssize_t clkpm_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf, size_t len)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);
	bool state_enable;

	if (kstrtobool(buf, &state_enable) < 0)
		return -EINVAL;

	down_read(&pci_bus_sem);
	mutex_lock(&aspm_lock);

	link->clkpm_disable = !state_enable;
	pcie_set_clkpm(link, policy_to_clkpm_state(link));

	mutex_unlock(&aspm_lock);
	up_read(&pci_bus_sem);

	return len;
}

static DEVICE_ATTR_RW(clkpm);
static DEVICE_ATTR_RW(l0s_aspm);
static DEVICE_ATTR_RW(l1_aspm);
static DEVICE_ATTR_RW(l1_1_aspm);
static DEVICE_ATTR_RW(l1_2_aspm);
static DEVICE_ATTR_RW(l1_1_pcipm);
static DEVICE_ATTR_RW(l1_2_pcipm);

static struct attribute *aspm_ctrl_attrs[] = {
	&dev_attr_clkpm.attr,
	&dev_attr_l0s_aspm.attr,
	&dev_attr_l1_aspm.attr,
	&dev_attr_l1_1_aspm.attr,
	&dev_attr_l1_2_aspm.attr,
	&dev_attr_l1_1_pcipm.attr,
	&dev_attr_l1_2_pcipm.attr,
	NULL
};

static umode_t aspm_ctrl_attrs_are_visible(struct kobject *kobj,
					   struct attribute *a, int n)
{
	struct device *dev = kobj_to_dev(kobj);
	struct pci_dev *pdev = to_pci_dev(dev);
	struct pcie_link_state *link = pcie_aspm_get_link(pdev);
	static const u8 aspm_state_map[] = {
		ASPM_STATE_L0S,
		ASPM_STATE_L1,
		ASPM_STATE_L1_1,
		ASPM_STATE_L1_2,
		ASPM_STATE_L1_1_PCIPM,
		ASPM_STATE_L1_2_PCIPM,
	};

	if (aspm_disabled || !link)
		return 0;

	if (n == 0)
		return link->clkpm_capable ? a->mode : 0;

	return link->aspm_capable & aspm_state_map[n - 1] ? a->mode : 0;
}

const struct attribute_group aspm_ctrl_attr_group = {
	.name = "link",
	.attrs = aspm_ctrl_attrs,
	.is_visible = aspm_ctrl_attrs_are_visible,
};

static int __init pcie_aspm_disable(char *str)
{
	if (!strcmp(str, "off")) {
		aspm_policy = POLICY_DEFAULT;
		aspm_disabled = 1;
		aspm_support_enabled = false;
		printk(KERN_INFO "PCIe ASPM is disabled\n");
	} else if (!strcmp(str, "force")) {
		aspm_force = 1;
		printk(KERN_INFO "PCIe ASPM is forcibly enabled\n");
	}
	return 1;
}

__setup("pcie_aspm=", pcie_aspm_disable);

void pcie_no_aspm(void)
{
	 
	if (!aspm_force) {
		aspm_policy = POLICY_DEFAULT;
		aspm_disabled = 1;
	}
}

bool pcie_aspm_support_enabled(void)
{
	return aspm_support_enabled;
}
