#ifndef _ASM_POWERPC_DEVICE_H
#define _ASM_POWERPC_DEVICE_H
struct device_node;
#ifdef CONFIG_PPC64
struct pci_dn;
struct iommu_table;
#endif
struct dev_archdata {
	dma_addr_t		dma_offset;
#ifdef CONFIG_PPC64
	struct iommu_table	*iommu_table_base;
#endif
#ifdef CONFIG_PPC64
	struct pci_dn		*pci_data;
#endif
#ifdef CONFIG_EEH
	struct eeh_dev		*edev;
#endif
#ifdef CONFIG_FAIL_IOMMU
	int fail_iommu;
#endif
#ifdef CONFIG_CXL_BASE
	struct cxl_context	*cxl_ctx;
#endif
#ifdef CONFIG_PCI_IOV
	void *iov_data;
#endif
};
struct pdev_archdata {
	u64 dma_mask;
	void *priv;
};
#endif  
