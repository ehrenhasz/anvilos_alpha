#ifndef _PCIE_IPROC_H
#define _PCIE_IPROC_H
enum iproc_pcie_type {
	IPROC_PCIE_PAXB_BCMA = 0,
	IPROC_PCIE_PAXB,
	IPROC_PCIE_PAXB_V2,
	IPROC_PCIE_PAXC,
	IPROC_PCIE_PAXC_V2,
};
struct iproc_pcie_ob {
	resource_size_t axi_offset;
	unsigned int nr_windows;
};
struct iproc_pcie_ib {
	unsigned int nr_regions;
};
struct iproc_pcie_ob_map;
struct iproc_pcie_ib_map;
struct iproc_msi;
struct iproc_pcie {
	struct device *dev;
	enum iproc_pcie_type type;
	u16 *reg_offsets;
	void __iomem *base;
	phys_addr_t base_addr;
	struct resource mem;
	struct phy *phy;
	int (*map_irq)(const struct pci_dev *, u8, u8);
	bool ep_is_internal;
	bool iproc_cfg_read;
	bool rej_unconfig_pf;
	bool has_apb_err_disable;
	bool fix_paxc_cap;
	bool need_ob_cfg;
	struct iproc_pcie_ob ob;
	const struct iproc_pcie_ob_map *ob_map;
	bool need_ib_cfg;
	struct iproc_pcie_ib ib;
	const struct iproc_pcie_ib_map *ib_map;
	bool need_msi_steer;
	struct iproc_msi *msi;
};
int iproc_pcie_setup(struct iproc_pcie *pcie, struct list_head *res);
void iproc_pcie_remove(struct iproc_pcie *pcie);
int iproc_pcie_shutdown(struct iproc_pcie *pcie);
#ifdef CONFIG_PCIE_IPROC_MSI
int iproc_msi_init(struct iproc_pcie *pcie, struct device_node *node);
void iproc_msi_exit(struct iproc_pcie *pcie);
#else
static inline int iproc_msi_init(struct iproc_pcie *pcie,
				 struct device_node *node)
{
	return -ENODEV;
}
static inline void iproc_msi_exit(struct iproc_pcie *pcie)
{
}
#endif
#endif  
