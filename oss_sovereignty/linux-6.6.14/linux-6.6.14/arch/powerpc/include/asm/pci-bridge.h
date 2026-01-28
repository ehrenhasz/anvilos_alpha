#ifndef _ASM_POWERPC_PCI_BRIDGE_H
#define _ASM_POWERPC_PCI_BRIDGE_H
#ifdef __KERNEL__
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/ioport.h>
#include <linux/numa.h>
#include <linux/iommu.h>
struct device_node;
struct pci_controller_ops {
	void		(*dma_dev_setup)(struct pci_dev *pdev);
	void		(*dma_bus_setup)(struct pci_bus *bus);
	bool		(*iommu_bypass_supported)(struct pci_dev *pdev,
				u64 mask);
	int		(*probe_mode)(struct pci_bus *bus);
	bool		(*enable_device_hook)(struct pci_dev *pdev);
	void		(*disable_device)(struct pci_dev *pdev);
	void		(*release_device)(struct pci_dev *pdev);
	resource_size_t (*window_alignment)(struct pci_bus *bus,
					    unsigned long type);
	void		(*setup_bridge)(struct pci_bus *bus,
					unsigned long type);
	void		(*reset_secondary_bus)(struct pci_dev *pdev);
#ifdef CONFIG_PCI_MSI
	int		(*setup_msi_irqs)(struct pci_dev *pdev,
					  int nvec, int type);
	void		(*teardown_msi_irqs)(struct pci_dev *pdev);
#endif
	void		(*shutdown)(struct pci_controller *hose);
	struct iommu_group *(*device_group)(struct pci_controller *hose,
					    struct pci_dev *pdev);
};
struct pci_controller {
	struct pci_bus *bus;
	char is_dynamic;
#ifdef CONFIG_PPC64
	int node;
#endif
	struct device_node *dn;
	struct list_head list_node;
	struct device *parent;
	int first_busno;
	int last_busno;
	int self_busno;
	struct resource busn;
	void __iomem *io_base_virt;
#ifdef CONFIG_PPC64
	void __iomem *io_base_alloc;
#endif
	resource_size_t io_base_phys;
	resource_size_t pci_io_size;
	resource_size_t	isa_mem_phys;
	resource_size_t	isa_mem_size;
	struct pci_controller_ops controller_ops;
	struct pci_ops *ops;
	unsigned int __iomem *cfg_addr;
	void __iomem *cfg_data;
#define PPC_INDIRECT_TYPE_SET_CFG_TYPE		0x00000001
#define PPC_INDIRECT_TYPE_EXT_REG		0x00000002
#define PPC_INDIRECT_TYPE_SURPRESS_PRIMARY_BUS	0x00000004
#define PPC_INDIRECT_TYPE_NO_PCIE_LINK		0x00000008
#define PPC_INDIRECT_TYPE_BIG_ENDIAN		0x00000010
#define PPC_INDIRECT_TYPE_BROKEN_MRM		0x00000020
#define PPC_INDIRECT_TYPE_FSL_CFG_REG_LINK	0x00000040
	u32 indirect_type;
	struct resource	io_resource;
	struct resource mem_resources[3];
	resource_size_t mem_offset[3];
	int global_number;		 
	resource_size_t dma_window_base_cur;
	resource_size_t dma_window_size;
#ifdef CONFIG_PPC64
	unsigned long buid;
	struct pci_dn *pci_data;
#endif	 
	void *private_data;
	struct irq_domain	*dev_domain;
	struct irq_domain	*msi_domain;
	struct fwnode_handle	*fwnode;
	struct iommu_device	iommu;
};
extern int early_read_config_byte(struct pci_controller *hose, int bus,
			int dev_fn, int where, u8 *val);
extern int early_read_config_word(struct pci_controller *hose, int bus,
			int dev_fn, int where, u16 *val);
extern int early_read_config_dword(struct pci_controller *hose, int bus,
			int dev_fn, int where, u32 *val);
extern int early_write_config_byte(struct pci_controller *hose, int bus,
			int dev_fn, int where, u8 val);
extern int early_write_config_word(struct pci_controller *hose, int bus,
			int dev_fn, int where, u16 val);
extern int early_write_config_dword(struct pci_controller *hose, int bus,
			int dev_fn, int where, u32 val);
extern int early_find_capability(struct pci_controller *hose, int bus,
				 int dev_fn, int cap);
extern void setup_indirect_pci(struct pci_controller* hose,
			       resource_size_t cfg_addr,
			       resource_size_t cfg_data, u32 flags);
extern int indirect_read_config(struct pci_bus *bus, unsigned int devfn,
				int offset, int len, u32 *val);
extern int __indirect_read_config(struct pci_controller *hose,
				  unsigned char bus_number, unsigned int devfn,
				  int offset, int len, u32 *val);
extern int indirect_write_config(struct pci_bus *bus, unsigned int devfn,
				 int offset, int len, u32 val);
static inline struct pci_controller *pci_bus_to_host(const struct pci_bus *bus)
{
	return bus->sysdata;
}
#ifdef CONFIG_PPC_PMAC
extern int pci_device_from_OF_node(struct device_node *node,
				   u8 *bus, u8 *devfn);
#endif
#ifndef CONFIG_PPC64
#ifdef CONFIG_PPC_PCI_OF_BUS_MAP
extern void pci_create_OF_bus_map(void);
#else
static inline void pci_create_OF_bus_map(void) {}
#endif
#else	 
struct iommu_table;
struct pci_dn {
	int     flags;
#define PCI_DN_FLAG_IOV_VF	0x01
#define PCI_DN_FLAG_DEAD	0x02     
	int	busno;			 
	int	devfn;			 
	int	vendor_id;		 
	int	device_id;		 
	int	class_code;		 
	struct  pci_dn *parent;
	struct  pci_controller *phb;	 
	struct	iommu_table_group *table_group;	 
	int	pci_ext_config_space;	 
#ifdef CONFIG_EEH
	struct eeh_dev *edev;		 
#endif
#define IODA_INVALID_PE		0xFFFFFFFF
	unsigned int pe_number;
#ifdef CONFIG_PCI_IOV
	u16     vfs_expanded;		 
	u16     num_vfs;		 
	unsigned int *pe_num_map;	 
	bool    m64_single_mode;	 
#define IODA_INVALID_M64        (-1)
	int     (*m64_map)[PCI_SRIOV_NUM_BARS];	 
	int     last_allow_rc;			 
#endif  
	int	mps;			 
	struct list_head child_list;
	struct list_head list;
	struct resource holes[PCI_SRIOV_NUM_BARS];
};
#define PCI_DN(dn)	((struct pci_dn *) (dn)->data)
extern struct pci_dn *pci_get_pdn_by_devfn(struct pci_bus *bus,
					   int devfn);
extern struct pci_dn *pci_get_pdn(struct pci_dev *pdev);
extern struct pci_dn *pci_add_device_node_info(struct pci_controller *hose,
					       struct device_node *dn);
extern void pci_remove_device_node_info(struct device_node *dn);
#ifdef CONFIG_PCI_IOV
struct pci_dn *add_sriov_vf_pdns(struct pci_dev *pdev);
void remove_sriov_vf_pdns(struct pci_dev *pdev);
#endif
#if defined(CONFIG_EEH)
static inline struct eeh_dev *pdn_to_eeh_dev(struct pci_dn *pdn)
{
	return pdn ? pdn->edev : NULL;
}
#else
#define pdn_to_eeh_dev(x)	(NULL)
#endif
extern struct pci_bus *pci_find_bus_by_node(struct device_node *dn);
extern void pci_hp_remove_devices(struct pci_bus *bus);
extern void pci_hp_add_devices(struct pci_bus *bus);
extern int pcibios_unmap_io_space(struct pci_bus *bus);
extern int pcibios_map_io_space(struct pci_bus *bus);
#ifdef CONFIG_NUMA
#define PHB_SET_NODE(PHB, NODE)		((PHB)->node = (NODE))
#else
#define PHB_SET_NODE(PHB, NODE)		((PHB)->node = NUMA_NO_NODE)
#endif
#endif	 
extern struct pci_controller *pci_find_hose_for_OF_device(
			struct device_node* node);
extern struct pci_controller *pci_find_controller_for_domain(int domain_nr);
extern void pci_process_bridge_OF_ranges(struct pci_controller *hose,
			struct device_node *dev, int primary);
extern struct pci_controller *pcibios_alloc_controller(struct device_node *dev);
extern void pcibios_free_controller(struct pci_controller *phb);
extern void pcibios_free_controller_deferred(struct pci_host_bridge *bridge);
#ifdef CONFIG_PCI
extern int pcibios_vaddr_is_ioport(void __iomem *address);
#else
static inline int pcibios_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif	 
#endif	 
#endif	 
