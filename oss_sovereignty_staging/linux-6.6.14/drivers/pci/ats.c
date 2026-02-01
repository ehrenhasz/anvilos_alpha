
 

#include <linux/export.h>
#include <linux/pci-ats.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include "pci.h"

void pci_ats_init(struct pci_dev *dev)
{
	int pos;

	if (pci_ats_disabled())
		return;

	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ATS);
	if (!pos)
		return;

	dev->ats_cap = pos;
}

 
bool pci_ats_supported(struct pci_dev *dev)
{
	if (!dev->ats_cap)
		return false;

	return (dev->untrusted == 0);
}
EXPORT_SYMBOL_GPL(pci_ats_supported);

 
int pci_enable_ats(struct pci_dev *dev, int ps)
{
	u16 ctrl;
	struct pci_dev *pdev;

	if (!pci_ats_supported(dev))
		return -EINVAL;

	if (WARN_ON(dev->ats_enabled))
		return -EBUSY;

	if (ps < PCI_ATS_MIN_STU)
		return -EINVAL;

	 
	ctrl = PCI_ATS_CTRL_ENABLE;
	if (dev->is_virtfn) {
		pdev = pci_physfn(dev);
		if (pdev->ats_stu != ps)
			return -EINVAL;
	} else {
		dev->ats_stu = ps;
		ctrl |= PCI_ATS_CTRL_STU(dev->ats_stu - PCI_ATS_MIN_STU);
	}
	pci_write_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, ctrl);

	dev->ats_enabled = 1;
	return 0;
}
EXPORT_SYMBOL_GPL(pci_enable_ats);

 
void pci_disable_ats(struct pci_dev *dev)
{
	u16 ctrl;

	if (WARN_ON(!dev->ats_enabled))
		return;

	pci_read_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, &ctrl);
	ctrl &= ~PCI_ATS_CTRL_ENABLE;
	pci_write_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, ctrl);

	dev->ats_enabled = 0;
}
EXPORT_SYMBOL_GPL(pci_disable_ats);

void pci_restore_ats_state(struct pci_dev *dev)
{
	u16 ctrl;

	if (!dev->ats_enabled)
		return;

	ctrl = PCI_ATS_CTRL_ENABLE;
	if (!dev->is_virtfn)
		ctrl |= PCI_ATS_CTRL_STU(dev->ats_stu - PCI_ATS_MIN_STU);
	pci_write_config_word(dev, dev->ats_cap + PCI_ATS_CTRL, ctrl);
}

 
int pci_ats_queue_depth(struct pci_dev *dev)
{
	u16 cap;

	if (!dev->ats_cap)
		return -EINVAL;

	if (dev->is_virtfn)
		return 0;

	pci_read_config_word(dev, dev->ats_cap + PCI_ATS_CAP, &cap);
	return PCI_ATS_CAP_QDEP(cap) ? PCI_ATS_CAP_QDEP(cap) : PCI_ATS_MAX_QDEP;
}

 
int pci_ats_page_aligned(struct pci_dev *pdev)
{
	u16 cap;

	if (!pdev->ats_cap)
		return 0;

	pci_read_config_word(pdev, pdev->ats_cap + PCI_ATS_CAP, &cap);

	if (cap & PCI_ATS_CAP_PAGE_ALIGNED)
		return 1;

	return 0;
}

#ifdef CONFIG_PCI_PRI
void pci_pri_init(struct pci_dev *pdev)
{
	u16 status;

	pdev->pri_cap = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_PRI);

	if (!pdev->pri_cap)
		return;

	pci_read_config_word(pdev, pdev->pri_cap + PCI_PRI_STATUS, &status);
	if (status & PCI_PRI_STATUS_PASID)
		pdev->pasid_required = 1;
}

 
int pci_enable_pri(struct pci_dev *pdev, u32 reqs)
{
	u16 control, status;
	u32 max_requests;
	int pri = pdev->pri_cap;

	 
	if (pdev->is_virtfn) {
		if (pci_physfn(pdev)->pri_enabled)
			return 0;
		return -EINVAL;
	}

	if (WARN_ON(pdev->pri_enabled))
		return -EBUSY;

	if (!pri)
		return -EINVAL;

	pci_read_config_word(pdev, pri + PCI_PRI_STATUS, &status);
	if (!(status & PCI_PRI_STATUS_STOPPED))
		return -EBUSY;

	pci_read_config_dword(pdev, pri + PCI_PRI_MAX_REQ, &max_requests);
	reqs = min(max_requests, reqs);
	pdev->pri_reqs_alloc = reqs;
	pci_write_config_dword(pdev, pri + PCI_PRI_ALLOC_REQ, reqs);

	control = PCI_PRI_CTRL_ENABLE;
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);

	pdev->pri_enabled = 1;

	return 0;
}

 
void pci_disable_pri(struct pci_dev *pdev)
{
	u16 control;
	int pri = pdev->pri_cap;

	 
	if (pdev->is_virtfn)
		return;

	if (WARN_ON(!pdev->pri_enabled))
		return;

	if (!pri)
		return;

	pci_read_config_word(pdev, pri + PCI_PRI_CTRL, &control);
	control &= ~PCI_PRI_CTRL_ENABLE;
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);

	pdev->pri_enabled = 0;
}
EXPORT_SYMBOL_GPL(pci_disable_pri);

 
void pci_restore_pri_state(struct pci_dev *pdev)
{
	u16 control = PCI_PRI_CTRL_ENABLE;
	u32 reqs = pdev->pri_reqs_alloc;
	int pri = pdev->pri_cap;

	if (pdev->is_virtfn)
		return;

	if (!pdev->pri_enabled)
		return;

	if (!pri)
		return;

	pci_write_config_dword(pdev, pri + PCI_PRI_ALLOC_REQ, reqs);
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);
}

 
int pci_reset_pri(struct pci_dev *pdev)
{
	u16 control;
	int pri = pdev->pri_cap;

	if (pdev->is_virtfn)
		return 0;

	if (WARN_ON(pdev->pri_enabled))
		return -EBUSY;

	if (!pri)
		return -EINVAL;

	control = PCI_PRI_CTRL_RESET;
	pci_write_config_word(pdev, pri + PCI_PRI_CTRL, control);

	return 0;
}

 
int pci_prg_resp_pasid_required(struct pci_dev *pdev)
{
	if (pdev->is_virtfn)
		pdev = pci_physfn(pdev);

	return pdev->pasid_required;
}

 
bool pci_pri_supported(struct pci_dev *pdev)
{
	 
	if (pci_physfn(pdev)->pri_cap)
		return true;
	return false;
}
EXPORT_SYMBOL_GPL(pci_pri_supported);
#endif  

#ifdef CONFIG_PCI_PASID
void pci_pasid_init(struct pci_dev *pdev)
{
	pdev->pasid_cap = pci_find_ext_capability(pdev, PCI_EXT_CAP_ID_PASID);
}

 
int pci_enable_pasid(struct pci_dev *pdev, int features)
{
	u16 control, supported;
	int pasid = pdev->pasid_cap;

	 
	if (pdev->is_virtfn) {
		if (pci_physfn(pdev)->pasid_enabled)
			return 0;
		return -EINVAL;
	}

	if (WARN_ON(pdev->pasid_enabled))
		return -EBUSY;

	if (!pdev->eetlp_prefix_path && !pdev->pasid_no_tlp)
		return -EINVAL;

	if (!pasid)
		return -EINVAL;

	if (!pci_acs_path_enabled(pdev, NULL, PCI_ACS_RR | PCI_ACS_UF))
		return -EINVAL;

	pci_read_config_word(pdev, pasid + PCI_PASID_CAP, &supported);
	supported &= PCI_PASID_CAP_EXEC | PCI_PASID_CAP_PRIV;

	 
	if ((supported & features) != features)
		return -EINVAL;

	control = PCI_PASID_CTRL_ENABLE | features;
	pdev->pasid_features = features;

	pci_write_config_word(pdev, pasid + PCI_PASID_CTRL, control);

	pdev->pasid_enabled = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(pci_enable_pasid);

 
void pci_disable_pasid(struct pci_dev *pdev)
{
	u16 control = 0;
	int pasid = pdev->pasid_cap;

	 
	if (pdev->is_virtfn)
		return;

	if (WARN_ON(!pdev->pasid_enabled))
		return;

	if (!pasid)
		return;

	pci_write_config_word(pdev, pasid + PCI_PASID_CTRL, control);

	pdev->pasid_enabled = 0;
}
EXPORT_SYMBOL_GPL(pci_disable_pasid);

 
void pci_restore_pasid_state(struct pci_dev *pdev)
{
	u16 control;
	int pasid = pdev->pasid_cap;

	if (pdev->is_virtfn)
		return;

	if (!pdev->pasid_enabled)
		return;

	if (!pasid)
		return;

	control = PCI_PASID_CTRL_ENABLE | pdev->pasid_features;
	pci_write_config_word(pdev, pasid + PCI_PASID_CTRL, control);
}

 
int pci_pasid_features(struct pci_dev *pdev)
{
	u16 supported;
	int pasid;

	if (pdev->is_virtfn)
		pdev = pci_physfn(pdev);

	pasid = pdev->pasid_cap;
	if (!pasid)
		return -EINVAL;

	pci_read_config_word(pdev, pasid + PCI_PASID_CAP, &supported);

	supported &= PCI_PASID_CAP_EXEC | PCI_PASID_CAP_PRIV;

	return supported;
}
EXPORT_SYMBOL_GPL(pci_pasid_features);

#define PASID_NUMBER_SHIFT	8
#define PASID_NUMBER_MASK	(0x1f << PASID_NUMBER_SHIFT)
 
int pci_max_pasids(struct pci_dev *pdev)
{
	u16 supported;
	int pasid;

	if (pdev->is_virtfn)
		pdev = pci_physfn(pdev);

	pasid = pdev->pasid_cap;
	if (!pasid)
		return -EINVAL;

	pci_read_config_word(pdev, pasid + PCI_PASID_CAP, &supported);

	supported = (supported & PASID_NUMBER_MASK) >> PASID_NUMBER_SHIFT;

	return (1 << supported);
}
EXPORT_SYMBOL_GPL(pci_max_pasids);
#endif  
