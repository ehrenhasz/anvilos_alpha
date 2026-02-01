
 

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/module.h>
#include <net/rtnetlink.h>

#include "iosm_ipc_imem.h"
#include "iosm_ipc_pcie.h"
#include "iosm_ipc_protocol.h"

MODULE_DESCRIPTION("IOSM Driver");
MODULE_LICENSE("GPL v2");

 
static guid_t wwan_acpi_guid = GUID_INIT(0xbad01b75, 0x22a8, 0x4f48, 0x87, 0x92,
				       0xbd, 0xde, 0x94, 0x67, 0x74, 0x7d);

static void ipc_pcie_resources_release(struct iosm_pcie *ipc_pcie)
{
	 
	ipc_release_irq(ipc_pcie);

	 
	iounmap(ipc_pcie->scratchpad);

	 
	iounmap(ipc_pcie->ipc_regs);

	 
	pci_release_regions(ipc_pcie->pci);
}

static void ipc_pcie_cleanup(struct iosm_pcie *ipc_pcie)
{
	 
	ipc_imem_cleanup(ipc_pcie->imem);

	ipc_pcie_resources_release(ipc_pcie);

	 
	pci_disable_device(ipc_pcie->pci);
}

static void ipc_pcie_deinit(struct iosm_pcie *ipc_pcie)
{
	kfree(ipc_pcie->imem);
	kfree(ipc_pcie);
}

static void ipc_pcie_remove(struct pci_dev *pci)
{
	struct iosm_pcie *ipc_pcie = pci_get_drvdata(pci);

	ipc_pcie_cleanup(ipc_pcie);

	ipc_pcie_deinit(ipc_pcie);
}

static int ipc_pcie_resources_request(struct iosm_pcie *ipc_pcie)
{
	struct pci_dev *pci = ipc_pcie->pci;
	u32 cap = 0;
	u32 ret;

	 
	ret = pci_request_regions(pci, "IOSM_IPC");
	if (ret) {
		dev_err(ipc_pcie->dev, "failed pci request regions");
		goto pci_request_region_fail;
	}

	 
	ipc_pcie->ipc_regs = pci_ioremap_bar(pci, ipc_pcie->ipc_regs_bar_nr);

	if (!ipc_pcie->ipc_regs) {
		dev_err(ipc_pcie->dev, "IPC REGS ioremap error");
		ret = -EBUSY;
		goto ipc_regs_remap_fail;
	}

	 
	ipc_pcie->scratchpad =
		pci_ioremap_bar(pci, ipc_pcie->scratchpad_bar_nr);

	if (!ipc_pcie->scratchpad) {
		dev_err(ipc_pcie->dev, "doorbell scratchpad ioremap error");
		ret = -EBUSY;
		goto scratch_remap_fail;
	}

	 
	ret = ipc_acquire_irq(ipc_pcie);
	if (ret) {
		dev_err(ipc_pcie->dev, "acquiring MSI irq failed!");
		goto irq_acquire_fail;
	}

	 
	pci_set_master(pci);

	 
	pcie_capability_read_dword(ipc_pcie->pci, PCI_EXP_DEVCAP2, &cap);
	if (cap & PCI_EXP_DEVCAP2_LTR)
		pcie_capability_set_word(ipc_pcie->pci, PCI_EXP_DEVCTL2,
					 PCI_EXP_DEVCTL2_LTR_EN);

	dev_dbg(ipc_pcie->dev, "link between AP and CP is fully on");

	return ret;

irq_acquire_fail:
	iounmap(ipc_pcie->scratchpad);
scratch_remap_fail:
	iounmap(ipc_pcie->ipc_regs);
ipc_regs_remap_fail:
	pci_release_regions(pci);
pci_request_region_fail:
	return ret;
}

bool ipc_pcie_check_aspm_enabled(struct iosm_pcie *ipc_pcie,
				 bool parent)
{
	struct pci_dev *pdev;
	u16 value = 0;
	u32 enabled;

	if (parent)
		pdev = ipc_pcie->pci->bus->self;
	else
		pdev = ipc_pcie->pci;

	pcie_capability_read_word(pdev, PCI_EXP_LNKCTL, &value);
	enabled = value & PCI_EXP_LNKCTL_ASPMC;
	dev_dbg(ipc_pcie->dev, "ASPM L1: 0x%04X 0x%03X", pdev->device, value);

	return (enabled == PCI_EXP_LNKCTL_ASPM_L1 ||
		enabled == PCI_EXP_LNKCTL_ASPMC);
}

bool ipc_pcie_check_data_link_active(struct iosm_pcie *ipc_pcie)
{
	struct pci_dev *parent;
	u16 link_status = 0;

	if (!ipc_pcie->pci->bus || !ipc_pcie->pci->bus->self) {
		dev_err(ipc_pcie->dev, "root port not found");
		return false;
	}

	parent = ipc_pcie->pci->bus->self;

	pcie_capability_read_word(parent, PCI_EXP_LNKSTA, &link_status);
	dev_dbg(ipc_pcie->dev, "Link status: 0x%04X", link_status);

	return link_status & PCI_EXP_LNKSTA_DLLLA;
}

static bool ipc_pcie_check_aspm_supported(struct iosm_pcie *ipc_pcie,
					  bool parent)
{
	struct pci_dev *pdev;
	u32 support;
	u32 cap = 0;

	if (parent)
		pdev = ipc_pcie->pci->bus->self;
	else
		pdev = ipc_pcie->pci;
	pcie_capability_read_dword(pdev, PCI_EXP_LNKCAP, &cap);
	support = u32_get_bits(cap, PCI_EXP_LNKCAP_ASPMS);
	if (support < PCI_EXP_LNKCTL_ASPM_L1) {
		dev_dbg(ipc_pcie->dev, "ASPM L1 not supported: 0x%04X",
			pdev->device);
		return false;
	}
	return true;
}

void ipc_pcie_config_aspm(struct iosm_pcie *ipc_pcie)
{
	bool parent_aspm_enabled, dev_aspm_enabled;

	 
	if (!ipc_pcie_check_aspm_supported(ipc_pcie, true) ||
	    !ipc_pcie_check_aspm_supported(ipc_pcie, false))
		return;

	parent_aspm_enabled = ipc_pcie_check_aspm_enabled(ipc_pcie, true);
	dev_aspm_enabled = ipc_pcie_check_aspm_enabled(ipc_pcie, false);

	dev_dbg(ipc_pcie->dev, "ASPM parent: %s device: %s",
		parent_aspm_enabled ? "Enabled" : "Disabled",
		dev_aspm_enabled ? "Enabled" : "Disabled");
}

 
static void ipc_pcie_config_init(struct iosm_pcie *ipc_pcie)
{
	 
	ipc_pcie->ipc_regs_bar_nr = IPC_DOORBELL_BAR0;

	 
	ipc_pcie->scratchpad_bar_nr = IPC_SCRATCHPAD_BAR2;
	ipc_pcie->doorbell_reg_offset = IPC_DOORBELL_CH_OFFSET;
	ipc_pcie->doorbell_write = IPC_WRITE_PTR_REG_0;
	ipc_pcie->doorbell_capture = IPC_CAPTURE_PTR_REG_0;
}

 
static enum ipc_pcie_sleep_state ipc_pcie_read_bios_cfg(struct device *dev)
{
	enum ipc_pcie_sleep_state sleep_state = IPC_PCIE_D0L12;
	union acpi_object *object;
	acpi_handle handle_acpi;

	handle_acpi = ACPI_HANDLE(dev);
	if (!handle_acpi) {
		pr_debug("pci device is NOT ACPI supporting device\n");
		goto default_ret;
	}

	object = acpi_evaluate_dsm(handle_acpi, &wwan_acpi_guid, 0, 3, NULL);
	if (!object)
		goto default_ret;

	if (object->integer.value == 3)
		sleep_state = IPC_PCIE_D3L2;

	ACPI_FREE(object);

default_ret:
	return sleep_state;
}

static int ipc_pcie_probe(struct pci_dev *pci,
			  const struct pci_device_id *pci_id)
{
	struct iosm_pcie *ipc_pcie = kzalloc(sizeof(*ipc_pcie), GFP_KERNEL);
	int ret;

	pr_debug("Probing device 0x%X from the vendor 0x%X", pci_id->device,
		 pci_id->vendor);

	if (!ipc_pcie)
		goto ret_fail;

	 
	ipc_pcie->dev = &pci->dev;

	 
	pci_set_drvdata(pci, ipc_pcie);

	 
	ipc_pcie->pci = pci;

	 
	ipc_pcie_config_init(ipc_pcie);

	 
	if (pci_enable_device(pci)) {
		dev_err(ipc_pcie->dev, "failed to enable the AP PCIe device");
		 
		goto pci_enable_fail;
	}

	ret = dma_set_mask(ipc_pcie->dev, DMA_BIT_MASK(64));
	if (ret) {
		dev_err(ipc_pcie->dev, "Could not set PCI DMA mask: %d", ret);
		goto set_mask_fail;
	}

	ipc_pcie_config_aspm(ipc_pcie);
	dev_dbg(ipc_pcie->dev, "PCIe device enabled.");

	 
	ipc_pcie->d3l2_support = ipc_pcie_read_bios_cfg(&pci->dev);

	ipc_pcie->suspend = 0;

	if (ipc_pcie_resources_request(ipc_pcie))
		goto resources_req_fail;

	 
	ipc_pcie->imem = ipc_imem_init(ipc_pcie, pci->device,
				       ipc_pcie->scratchpad, ipc_pcie->dev);
	if (!ipc_pcie->imem) {
		dev_err(ipc_pcie->dev, "failed to init imem");
		goto imem_init_fail;
	}

	return 0;

imem_init_fail:
	ipc_pcie_resources_release(ipc_pcie);
resources_req_fail:
set_mask_fail:
	pci_disable_device(pci);
pci_enable_fail:
	kfree(ipc_pcie);
ret_fail:
	return -EIO;
}

static const struct pci_device_id iosm_ipc_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_CP_DEVICE_7560_ID) },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, INTEL_CP_DEVICE_7360_ID) },
	{}
};
MODULE_DEVICE_TABLE(pci, iosm_ipc_ids);

 
static int __maybe_unused ipc_pcie_suspend_s2idle(struct iosm_pcie *ipc_pcie)
{
	ipc_cp_irq_sleep_control(ipc_pcie, IPC_MEM_DEV_PM_FORCE_SLEEP);

	 
	smp_mb__before_atomic();

	set_bit(0, &ipc_pcie->suspend);

	 
	smp_mb__after_atomic();

	ipc_imem_pm_s2idle_sleep(ipc_pcie->imem, true);

	return 0;
}

 
static int __maybe_unused ipc_pcie_resume_s2idle(struct iosm_pcie *ipc_pcie)
{
	ipc_cp_irq_sleep_control(ipc_pcie, IPC_MEM_DEV_PM_FORCE_ACTIVE);

	ipc_imem_pm_s2idle_sleep(ipc_pcie->imem, false);

	 
	smp_mb__before_atomic();

	clear_bit(0, &ipc_pcie->suspend);

	 
	smp_mb__after_atomic();
	return 0;
}

int __maybe_unused ipc_pcie_suspend(struct iosm_pcie *ipc_pcie)
{
	 
	ipc_imem_pm_suspend(ipc_pcie->imem);

	dev_dbg(ipc_pcie->dev, "SUSPEND done");
	return 0;
}

int __maybe_unused ipc_pcie_resume(struct iosm_pcie *ipc_pcie)
{
	 
	ipc_imem_pm_resume(ipc_pcie->imem);

	dev_dbg(ipc_pcie->dev, "RESUME done");
	return 0;
}

static int __maybe_unused ipc_pcie_suspend_cb(struct device *dev)
{
	struct iosm_pcie *ipc_pcie;
	struct pci_dev *pdev;

	pdev = to_pci_dev(dev);

	ipc_pcie = pci_get_drvdata(pdev);

	switch (ipc_pcie->d3l2_support) {
	case IPC_PCIE_D0L12:
		ipc_pcie_suspend_s2idle(ipc_pcie);
		break;
	case IPC_PCIE_D3L2:
		ipc_pcie_suspend(ipc_pcie);
		break;
	}

	return 0;
}

static int __maybe_unused ipc_pcie_resume_cb(struct device *dev)
{
	struct iosm_pcie *ipc_pcie;
	struct pci_dev *pdev;

	pdev = to_pci_dev(dev);

	ipc_pcie = pci_get_drvdata(pdev);

	switch (ipc_pcie->d3l2_support) {
	case IPC_PCIE_D0L12:
		ipc_pcie_resume_s2idle(ipc_pcie);
		break;
	case IPC_PCIE_D3L2:
		ipc_pcie_resume(ipc_pcie);
		break;
	}

	return 0;
}

static SIMPLE_DEV_PM_OPS(iosm_ipc_pm, ipc_pcie_suspend_cb, ipc_pcie_resume_cb);

static struct pci_driver iosm_ipc_driver = {
	.name = KBUILD_MODNAME,
	.probe = ipc_pcie_probe,
	.remove = ipc_pcie_remove,
	.driver = {
		.pm = &iosm_ipc_pm,
	},
	.id_table = iosm_ipc_ids,
};
module_pci_driver(iosm_ipc_driver);

int ipc_pcie_addr_map(struct iosm_pcie *ipc_pcie, unsigned char *data,
		      size_t size, dma_addr_t *mapping, int direction)
{
	if (ipc_pcie->pci) {
		*mapping = dma_map_single(&ipc_pcie->pci->dev, data, size,
					  direction);
		if (dma_mapping_error(&ipc_pcie->pci->dev, *mapping)) {
			dev_err(ipc_pcie->dev, "dma mapping failed");
			return -EINVAL;
		}
	}
	return 0;
}

void ipc_pcie_addr_unmap(struct iosm_pcie *ipc_pcie, size_t size,
			 dma_addr_t mapping, int direction)
{
	if (!mapping)
		return;
	if (ipc_pcie->pci)
		dma_unmap_single(&ipc_pcie->pci->dev, mapping, size, direction);
}

struct sk_buff *ipc_pcie_alloc_local_skb(struct iosm_pcie *ipc_pcie,
					 gfp_t flags, size_t size)
{
	struct sk_buff *skb;

	if (!ipc_pcie || !size) {
		pr_err("invalid pcie object or size");
		return NULL;
	}

	skb = __netdev_alloc_skb(NULL, size, flags);
	if (!skb)
		return NULL;

	IPC_CB(skb)->op_type = (u8)UL_DEFAULT;
	IPC_CB(skb)->mapping = 0;

	return skb;
}

struct sk_buff *ipc_pcie_alloc_skb(struct iosm_pcie *ipc_pcie, size_t size,
				   gfp_t flags, dma_addr_t *mapping,
				   int direction, size_t headroom)
{
	struct sk_buff *skb = ipc_pcie_alloc_local_skb(ipc_pcie, flags,
						       size + headroom);
	if (!skb)
		return NULL;

	if (headroom)
		skb_reserve(skb, headroom);

	if (ipc_pcie_addr_map(ipc_pcie, skb->data, size, mapping, direction)) {
		dev_kfree_skb(skb);
		return NULL;
	}

	BUILD_BUG_ON(sizeof(*IPC_CB(skb)) > sizeof(skb->cb));

	 
	IPC_CB(skb)->mapping = *mapping;
	IPC_CB(skb)->direction = direction;
	IPC_CB(skb)->len = size;

	return skb;
}

void ipc_pcie_kfree_skb(struct iosm_pcie *ipc_pcie, struct sk_buff *skb)
{
	if (!skb)
		return;

	ipc_pcie_addr_unmap(ipc_pcie, IPC_CB(skb)->len, IPC_CB(skb)->mapping,
			    IPC_CB(skb)->direction);
	IPC_CB(skb)->mapping = 0;
	dev_kfree_skb(skb);
}
