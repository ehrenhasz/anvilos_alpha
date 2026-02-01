
 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include "../pci.h"

 
static struct pci_dev *pci_upstream_ptm(struct pci_dev *dev)
{
	struct pci_dev *ups = pci_upstream_bridge(dev);

	 
	if (ups && pci_pcie_type(ups) == PCI_EXP_TYPE_DOWNSTREAM)
		ups = pci_upstream_bridge(ups);

	if (ups && ups->ptm_cap)
		return ups;

	return NULL;
}

 
void pci_ptm_init(struct pci_dev *dev)
{
	u16 ptm;
	u32 cap;
	struct pci_dev *ups;

	if (!pci_is_pcie(dev))
		return;

	ptm = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_PTM);
	if (!ptm)
		return;

	dev->ptm_cap = ptm;
	pci_add_ext_cap_save_buffer(dev, PCI_EXT_CAP_ID_PTM, sizeof(u32));

	pci_read_config_dword(dev, ptm + PCI_PTM_CAP, &cap);
	dev->ptm_granularity = (cap & PCI_PTM_GRANULARITY_MASK) >> 8;

	 
	ups = pci_upstream_ptm(dev);
	if (ups) {
		if (ups->ptm_granularity == 0)
			dev->ptm_granularity = 0;
		else if (ups->ptm_granularity > dev->ptm_granularity)
			dev->ptm_granularity = ups->ptm_granularity;
	} else if (cap & PCI_PTM_CAP_ROOT) {
		dev->ptm_root = 1;
	} else if (pci_pcie_type(dev) == PCI_EXP_TYPE_RC_END) {

		 
		dev->ptm_granularity = 0;
	}

	if (pci_pcie_type(dev) == PCI_EXP_TYPE_ROOT_PORT ||
	    pci_pcie_type(dev) == PCI_EXP_TYPE_UPSTREAM)
		pci_enable_ptm(dev, NULL);
}

void pci_save_ptm_state(struct pci_dev *dev)
{
	u16 ptm = dev->ptm_cap;
	struct pci_cap_saved_state *save_state;
	u32 *cap;

	if (!ptm)
		return;

	save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_PTM);
	if (!save_state)
		return;

	cap = (u32 *)&save_state->cap.data[0];
	pci_read_config_dword(dev, ptm + PCI_PTM_CTRL, cap);
}

void pci_restore_ptm_state(struct pci_dev *dev)
{
	u16 ptm = dev->ptm_cap;
	struct pci_cap_saved_state *save_state;
	u32 *cap;

	if (!ptm)
		return;

	save_state = pci_find_saved_ext_cap(dev, PCI_EXT_CAP_ID_PTM);
	if (!save_state)
		return;

	cap = (u32 *)&save_state->cap.data[0];
	pci_write_config_dword(dev, ptm + PCI_PTM_CTRL, *cap);
}

 
static int __pci_enable_ptm(struct pci_dev *dev)
{
	u16 ptm = dev->ptm_cap;
	struct pci_dev *ups;
	u32 ctrl;

	if (!ptm)
		return -EINVAL;

	 
	if (!dev->ptm_root) {
		ups = pci_upstream_ptm(dev);
		if (!ups || !ups->ptm_enabled)
			return -EINVAL;
	}

	pci_read_config_dword(dev, ptm + PCI_PTM_CTRL, &ctrl);

	ctrl |= PCI_PTM_CTRL_ENABLE;
	ctrl &= ~PCI_PTM_GRANULARITY_MASK;
	ctrl |= dev->ptm_granularity << 8;
	if (dev->ptm_root)
		ctrl |= PCI_PTM_CTRL_ROOT;

	pci_write_config_dword(dev, ptm + PCI_PTM_CTRL, ctrl);
	return 0;
}

 
int pci_enable_ptm(struct pci_dev *dev, u8 *granularity)
{
	int rc;
	char clock_desc[8];

	rc = __pci_enable_ptm(dev);
	if (rc)
		return rc;

	dev->ptm_enabled = 1;

	if (granularity)
		*granularity = dev->ptm_granularity;

	switch (dev->ptm_granularity) {
	case 0:
		snprintf(clock_desc, sizeof(clock_desc), "unknown");
		break;
	case 255:
		snprintf(clock_desc, sizeof(clock_desc), ">254ns");
		break;
	default:
		snprintf(clock_desc, sizeof(clock_desc), "%uns",
			 dev->ptm_granularity);
		break;
	}
	pci_info(dev, "PTM enabled%s, %s granularity\n",
		 dev->ptm_root ? " (root)" : "", clock_desc);

	return 0;
}
EXPORT_SYMBOL(pci_enable_ptm);

static void __pci_disable_ptm(struct pci_dev *dev)
{
	u16 ptm = dev->ptm_cap;
	u32 ctrl;

	if (!ptm)
		return;

	pci_read_config_dword(dev, ptm + PCI_PTM_CTRL, &ctrl);
	ctrl &= ~(PCI_PTM_CTRL_ENABLE | PCI_PTM_CTRL_ROOT);
	pci_write_config_dword(dev, ptm + PCI_PTM_CTRL, ctrl);
}

 
void pci_disable_ptm(struct pci_dev *dev)
{
	if (dev->ptm_enabled) {
		__pci_disable_ptm(dev);
		dev->ptm_enabled = 0;
	}
}
EXPORT_SYMBOL(pci_disable_ptm);

 
void pci_suspend_ptm(struct pci_dev *dev)
{
	if (dev->ptm_enabled)
		__pci_disable_ptm(dev);
}

 
void pci_resume_ptm(struct pci_dev *dev)
{
	if (dev->ptm_enabled)
		__pci_enable_ptm(dev);
}

bool pcie_ptm_enabled(struct pci_dev *dev)
{
	if (!dev)
		return false;

	return dev->ptm_enabled;
}
EXPORT_SYMBOL(pcie_ptm_enabled);
