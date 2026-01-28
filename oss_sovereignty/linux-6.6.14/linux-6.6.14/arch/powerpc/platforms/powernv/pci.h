#ifndef __POWERNV_PCI_H
#define __POWERNV_PCI_H
#include <linux/compiler.h>		 
#include <linux/iommu.h>
#include <asm/iommu.h>
#include <asm/msi_bitmap.h>
struct pci_dn;
enum pnv_phb_type {
	PNV_PHB_IODA2,
	PNV_PHB_NPU_OCAPI,
};
enum pnv_phb_model {
	PNV_PHB_MODEL_UNKNOWN,
	PNV_PHB_MODEL_P7IOC,
	PNV_PHB_MODEL_PHB3,
};
#define PNV_PCI_DIAG_BUF_SIZE	8192
#define PNV_IODA_PE_DEV		(1 << 0)	 
#define PNV_IODA_PE_BUS		(1 << 1)	 
#define PNV_IODA_PE_BUS_ALL	(1 << 2)	 
#define PNV_IODA_PE_MASTER	(1 << 3)	 
#define PNV_IODA_PE_SLAVE	(1 << 4)	 
#define PNV_IODA_PE_VF		(1 << 5)	 
#define PNV_IODA_STOPPED_STATE	0x8000000000000000
struct pnv_phb;
struct pnv_ioda_pe {
	unsigned long		flags;
	struct pnv_phb		*phb;
	int			device_count;
#ifdef CONFIG_PCI_IOV
	struct pci_dev          *parent_dev;
#endif
	struct pci_dev		*pdev;
	struct pci_bus		*pbus;
	unsigned int		rid;
	unsigned int		pe_number;
	struct iommu_table_group table_group;
	bool			tce_bypass_enabled;
	uint64_t		tce_bypass_base;
	bool			dma_setup_done;
	int			mve_number;
	struct pnv_ioda_pe	*master;
	struct list_head	slaves;
	struct list_head	list;
};
#define PNV_PHB_FLAG_EEH	(1 << 0)
struct pnv_phb {
	struct pci_controller	*hose;
	enum pnv_phb_type	type;
	enum pnv_phb_model	model;
	u64			hub_id;
	u64			opal_id;
	int			flags;
	void __iomem		*regs;
	u64			regs_phys;
	spinlock_t		lock;
#ifdef CONFIG_DEBUG_FS
	int			has_dbgfs;
	struct dentry		*dbgfs;
#endif
	unsigned int		msi_base;
	struct msi_bitmap	msi_bmp;
	int (*init_m64)(struct pnv_phb *phb);
	int (*get_pe_state)(struct pnv_phb *phb, int pe_no);
	void (*freeze_pe)(struct pnv_phb *phb, int pe_no);
	int (*unfreeze_pe)(struct pnv_phb *phb, int pe_no, int opt);
	struct {
		unsigned int		total_pe_num;
		unsigned int		reserved_pe_idx;
		unsigned int		root_pe_idx;
		unsigned int		m32_size;
		unsigned int		m32_segsize;
		unsigned int		m32_pci_base;
		unsigned int		m64_bar_idx;
		unsigned long		m64_size;
		unsigned long		m64_segsize;
		unsigned long		m64_base;
#define MAX_M64_BARS 64
		unsigned long		m64_bar_alloc;
		unsigned int		io_size;
		unsigned int		io_segsize;
		unsigned int		io_pci_base;
		struct mutex		pe_alloc_mutex;
		unsigned long		*pe_alloc;
		struct pnv_ioda_pe	*pe_array;
		unsigned int		*m64_segmap;
		unsigned int		*m32_segmap;
		unsigned int		*io_segmap;
		int			irq_chip_init;
		struct irq_chip		irq_chip;
		struct list_head	pe_list;
		struct mutex            pe_list_mutex;
		unsigned int		pe_rmap[0x10000];
	} ioda;
	unsigned int		diag_data_size;
	u8			*diag_data;
};
static inline bool pnv_pci_is_m64(struct pnv_phb *phb, struct resource *r)
{
	return (r->start >= phb->ioda.m64_base &&
		r->start < (phb->ioda.m64_base + phb->ioda.m64_size));
}
static inline bool pnv_pci_is_m64_flags(unsigned long resource_flags)
{
	unsigned long flags = (IORESOURCE_MEM_64 | IORESOURCE_PREFETCH);
	return (resource_flags & flags) == flags;
}
int pnv_ioda_configure_pe(struct pnv_phb *phb, struct pnv_ioda_pe *pe);
int pnv_ioda_deconfigure_pe(struct pnv_phb *phb, struct pnv_ioda_pe *pe);
void pnv_pci_ioda2_setup_dma_pe(struct pnv_phb *phb, struct pnv_ioda_pe *pe);
void pnv_pci_ioda2_release_pe_dma(struct pnv_ioda_pe *pe);
struct pnv_ioda_pe *pnv_ioda_alloc_pe(struct pnv_phb *phb, int count);
void pnv_ioda_free_pe(struct pnv_ioda_pe *pe);
#ifdef CONFIG_PCI_IOV
struct pnv_iov_data {
	u16     num_vfs;
	struct pnv_ioda_pe *vf_pe_arr;
	bool    m64_single_mode[PCI_SRIOV_NUM_BARS];
	bool    need_shift;
	DECLARE_BITMAP(used_m64_bar_mask, MAX_M64_BARS);
	struct resource holes[PCI_SRIOV_NUM_BARS];
};
static inline struct pnv_iov_data *pnv_iov_get(struct pci_dev *pdev)
{
	return pdev->dev.archdata.iov_data;
}
void pnv_pci_ioda_fixup_iov(struct pci_dev *pdev);
resource_size_t pnv_pci_iov_resource_alignment(struct pci_dev *pdev, int resno);
int pnv_pcibios_sriov_enable(struct pci_dev *pdev, u16 num_vfs);
int pnv_pcibios_sriov_disable(struct pci_dev *pdev);
#endif  
extern struct pci_ops pnv_pci_ops;
void pnv_pci_dump_phb_diag_data(struct pci_controller *hose,
				unsigned char *log_buff);
int pnv_pci_cfg_read(struct pci_dn *pdn,
		     int where, int size, u32 *val);
int pnv_pci_cfg_write(struct pci_dn *pdn,
		      int where, int size, u32 val);
extern struct iommu_table *pnv_pci_table_alloc(int nid);
extern void pnv_pci_init_ioda_hub(struct device_node *np);
extern void pnv_pci_init_ioda2_phb(struct device_node *np);
extern void pnv_pci_init_npu2_opencapi_phb(struct device_node *np);
extern void pnv_pci_reset_secondary_bus(struct pci_dev *dev);
extern int pnv_eeh_phb_reset(struct pci_controller *hose, int option);
extern struct pnv_ioda_pe *pnv_pci_bdfn_to_pe(struct pnv_phb *phb, u16 bdfn);
extern struct pnv_ioda_pe *pnv_ioda_get_pe(struct pci_dev *dev);
extern void pnv_set_msi_irq_chip(struct pnv_phb *phb, unsigned int virq);
extern unsigned long pnv_pci_ioda2_get_table_size(__u32 page_shift,
		__u64 window_size, __u32 levels);
extern int pnv_eeh_post_init(void);
__printf(3, 4)
extern void pe_level_printk(const struct pnv_ioda_pe *pe, const char *level,
			    const char *fmt, ...);
#define pe_err(pe, fmt, ...)					\
	pe_level_printk(pe, KERN_ERR, fmt, ##__VA_ARGS__)
#define pe_warn(pe, fmt, ...)					\
	pe_level_printk(pe, KERN_WARNING, fmt, ##__VA_ARGS__)
#define pe_info(pe, fmt, ...)					\
	pe_level_printk(pe, KERN_INFO, fmt, ##__VA_ARGS__)
#define POWERNV_IOMMU_DEFAULT_LEVELS	2
#define POWERNV_IOMMU_MAX_LEVELS	5
extern int pnv_tce_build(struct iommu_table *tbl, long index, long npages,
		unsigned long uaddr, enum dma_data_direction direction,
		unsigned long attrs);
extern void pnv_tce_free(struct iommu_table *tbl, long index, long npages);
extern int pnv_tce_xchg(struct iommu_table *tbl, long index,
		unsigned long *hpa, enum dma_data_direction *direction);
extern __be64 *pnv_tce_useraddrptr(struct iommu_table *tbl, long index,
		bool alloc);
extern unsigned long pnv_tce_get(struct iommu_table *tbl, long index);
extern long pnv_pci_ioda2_table_alloc_pages(int nid, __u64 bus_offset,
		__u32 page_shift, __u64 window_size, __u32 levels,
		bool alloc_userspace_copy, struct iommu_table *tbl);
extern void pnv_pci_ioda2_table_free_pages(struct iommu_table *tbl);
extern long pnv_pci_link_table_and_group(int node, int num,
		struct iommu_table *tbl,
		struct iommu_table_group *table_group);
extern void pnv_pci_unlink_table_and_group(struct iommu_table *tbl,
		struct iommu_table_group *table_group);
extern void pnv_pci_setup_iommu_table(struct iommu_table *tbl,
		void *tce_mem, u64 tce_size,
		u64 dma_offset, unsigned int page_shift);
extern unsigned long pnv_ioda_parse_tce_sizes(struct pnv_phb *phb);
static inline struct pnv_phb *pci_bus_to_pnvhb(struct pci_bus *bus)
{
	struct pci_controller *hose = bus->sysdata;
	if (hose)
		return hose->private_data;
	return NULL;
}
#endif  
