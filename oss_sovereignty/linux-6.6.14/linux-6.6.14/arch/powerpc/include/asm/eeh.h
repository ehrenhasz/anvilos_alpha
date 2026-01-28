#ifndef _POWERPC_EEH_H
#define _POWERPC_EEH_H
#ifdef __KERNEL__
#include <linux/init.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/atomic.h>
#include <uapi/asm/eeh.h>
struct pci_dev;
struct pci_bus;
struct pci_dn;
#ifdef CONFIG_EEH
#define EEH_ENABLED		0x01	 
#define EEH_FORCE_DISABLED	0x02	 
#define EEH_PROBE_MODE_DEV	0x04	 
#define EEH_PROBE_MODE_DEVTREE	0x08	 
#define EEH_ENABLE_IO_FOR_LOG	0x20	 
#define EEH_EARLY_DUMP_LOG	0x40	 
#define EEH_PE_RST_HOLD_TIME		250
#define EEH_PE_RST_SETTLE_TIME		1800
#define EEH_PE_INVALID	(1 << 0)	 
#define EEH_PE_PHB	(1 << 1)	 
#define EEH_PE_DEVICE 	(1 << 2)	 
#define EEH_PE_BUS	(1 << 3)	 
#define EEH_PE_VF	(1 << 4)	 
#define EEH_PE_ISOLATED		(1 << 0)	 
#define EEH_PE_RECOVERING	(1 << 1)	 
#define EEH_PE_CFG_BLOCKED	(1 << 2)	 
#define EEH_PE_RESET		(1 << 3)	 
#define EEH_PE_KEEP		(1 << 8)	 
#define EEH_PE_CFG_RESTRICTED	(1 << 9)	 
#define EEH_PE_REMOVED		(1 << 10)	 
#define EEH_PE_PRI_BUS		(1 << 11)	 
struct eeh_pe {
	int type;			 
	int state;			 
	int addr;			 
	struct pci_controller *phb;	 
	struct pci_bus *bus;		 
	int check_count;		 
	int freeze_count;		 
	time64_t tstamp;		 
	int false_positives;		 
	atomic_t pass_dev_cnt;		 
	struct eeh_pe *parent;		 
	void *data;			 
	struct list_head child_list;	 
	struct list_head child;		 
	struct list_head edevs;		 
#ifdef CONFIG_STACKTRACE
	unsigned long stack_trace[64];
	int trace_entries;
#endif  
};
#define eeh_pe_for_each_dev(pe, edev, tmp) \
		list_for_each_entry_safe(edev, tmp, &pe->edevs, entry)
#define eeh_for_each_pe(root, pe) \
	for (pe = root; pe; pe = eeh_pe_next(pe, root))
static inline bool eeh_pe_passed(struct eeh_pe *pe)
{
	return pe ? !!atomic_read(&pe->pass_dev_cnt) : false;
}
#define EEH_DEV_BRIDGE		(1 << 0)	 
#define EEH_DEV_ROOT_PORT	(1 << 1)	 
#define EEH_DEV_DS_PORT		(1 << 2)	 
#define EEH_DEV_IRQ_DISABLED	(1 << 3)	 
#define EEH_DEV_DISCONNECTED	(1 << 4)	 
#define EEH_DEV_NO_HANDLER	(1 << 8)	 
#define EEH_DEV_SYSFS		(1 << 9)	 
#define EEH_DEV_REMOVED		(1 << 10)	 
struct eeh_dev {
	int mode;			 
	int bdfn;			 
	struct pci_controller *controller;
	int pe_config_addr;		 
	u32 config_space[16];		 
	int pcix_cap;			 
	int pcie_cap;			 
	int aer_cap;			 
	int af_cap;			 
	struct eeh_pe *pe;		 
	struct list_head entry;		 
	struct list_head rmv_entry;	 
	struct pci_dn *pdn;		 
	struct pci_dev *pdev;		 
	bool in_error;			 
	struct pci_dev *physfn;		 
	int vf_index;			 
};
#define EEH_EDEV_PRINT(level, edev, fmt, ...) \
	pr_##level("PCI %04x:%02x:%02x.%x#%04x: EEH: " fmt, \
	(edev)->controller->global_number, PCI_BUSNO((edev)->bdfn), \
	PCI_SLOT((edev)->bdfn), PCI_FUNC((edev)->bdfn), \
	((edev)->pe ? (edev)->pe_config_addr : 0xffff), ##__VA_ARGS__)
#define eeh_edev_dbg(edev, fmt, ...) EEH_EDEV_PRINT(debug, (edev), fmt, ##__VA_ARGS__)
#define eeh_edev_info(edev, fmt, ...) EEH_EDEV_PRINT(info, (edev), fmt, ##__VA_ARGS__)
#define eeh_edev_warn(edev, fmt, ...) EEH_EDEV_PRINT(warn, (edev), fmt, ##__VA_ARGS__)
#define eeh_edev_err(edev, fmt, ...) EEH_EDEV_PRINT(err, (edev), fmt, ##__VA_ARGS__)
static inline struct pci_dn *eeh_dev_to_pdn(struct eeh_dev *edev)
{
	return edev ? edev->pdn : NULL;
}
static inline struct pci_dev *eeh_dev_to_pci_dev(struct eeh_dev *edev)
{
	return edev ? edev->pdev : NULL;
}
static inline struct eeh_pe *eeh_dev_to_pe(struct eeh_dev* edev)
{
	return edev ? edev->pe : NULL;
}
enum {
	EEH_NEXT_ERR_NONE = 0,
	EEH_NEXT_ERR_INF,
	EEH_NEXT_ERR_FROZEN_PE,
	EEH_NEXT_ERR_FENCED_PHB,
	EEH_NEXT_ERR_DEAD_PHB,
	EEH_NEXT_ERR_DEAD_IOC
};
#define EEH_OPT_DISABLE		0	 
#define EEH_OPT_ENABLE		1	 
#define EEH_OPT_THAW_MMIO	2	 
#define EEH_OPT_THAW_DMA	3	 
#define EEH_OPT_FREEZE_PE	4	 
#define EEH_STATE_UNAVAILABLE	(1 << 0)	 
#define EEH_STATE_NOT_SUPPORT	(1 << 1)	 
#define EEH_STATE_RESET_ACTIVE	(1 << 2)	 
#define EEH_STATE_MMIO_ACTIVE	(1 << 3)	 
#define EEH_STATE_DMA_ACTIVE	(1 << 4)	 
#define EEH_STATE_MMIO_ENABLED	(1 << 5)	 
#define EEH_STATE_DMA_ENABLED	(1 << 6)	 
#define EEH_RESET_DEACTIVATE	0	 
#define EEH_RESET_HOT		1	 
#define EEH_RESET_FUNDAMENTAL	3	 
#define EEH_LOG_TEMP		1	 
#define EEH_LOG_PERM		2	 
struct eeh_ops {
	char *name;
	struct eeh_dev *(*probe)(struct pci_dev *pdev);
	int (*set_option)(struct eeh_pe *pe, int option);
	int (*get_state)(struct eeh_pe *pe, int *delay);
	int (*reset)(struct eeh_pe *pe, int option);
	int (*get_log)(struct eeh_pe *pe, int severity, char *drv_log, unsigned long len);
	int (*configure_bridge)(struct eeh_pe *pe);
	int (*err_inject)(struct eeh_pe *pe, int type, int func,
			  unsigned long addr, unsigned long mask);
	int (*read_config)(struct eeh_dev *edev, int where, int size, u32 *val);
	int (*write_config)(struct eeh_dev *edev, int where, int size, u32 val);
	int (*next_error)(struct eeh_pe **pe);
	int (*restore_config)(struct eeh_dev *edev);
	int (*notify_resume)(struct eeh_dev *edev);
};
extern int eeh_subsystem_flags;
extern u32 eeh_max_freezes;
extern bool eeh_debugfs_no_recover;
extern struct eeh_ops *eeh_ops;
extern raw_spinlock_t confirm_error_lock;
static inline void eeh_add_flag(int flag)
{
	eeh_subsystem_flags |= flag;
}
static inline void eeh_clear_flag(int flag)
{
	eeh_subsystem_flags &= ~flag;
}
static inline bool eeh_has_flag(int flag)
{
        return !!(eeh_subsystem_flags & flag);
}
static inline bool eeh_enabled(void)
{
	return eeh_has_flag(EEH_ENABLED) && !eeh_has_flag(EEH_FORCE_DISABLED);
}
static inline void eeh_serialize_lock(unsigned long *flags)
{
	raw_spin_lock_irqsave(&confirm_error_lock, *flags);
}
static inline void eeh_serialize_unlock(unsigned long flags)
{
	raw_spin_unlock_irqrestore(&confirm_error_lock, flags);
}
static inline bool eeh_state_active(int state)
{
	return (state & (EEH_STATE_MMIO_ACTIVE | EEH_STATE_DMA_ACTIVE))
	== (EEH_STATE_MMIO_ACTIVE | EEH_STATE_DMA_ACTIVE);
}
typedef void (*eeh_edev_traverse_func)(struct eeh_dev *edev, void *flag);
typedef void *(*eeh_pe_traverse_func)(struct eeh_pe *pe, void *flag);
void eeh_set_pe_aux_size(int size);
int eeh_phb_pe_create(struct pci_controller *phb);
int eeh_wait_state(struct eeh_pe *pe, int max_wait);
struct eeh_pe *eeh_phb_pe_get(struct pci_controller *phb);
struct eeh_pe *eeh_pe_next(struct eeh_pe *pe, struct eeh_pe *root);
struct eeh_pe *eeh_pe_get(struct pci_controller *phb, int pe_no);
int eeh_pe_tree_insert(struct eeh_dev *edev, struct eeh_pe *new_pe_parent);
int eeh_pe_tree_remove(struct eeh_dev *edev);
void eeh_pe_update_time_stamp(struct eeh_pe *pe);
void *eeh_pe_traverse(struct eeh_pe *root,
		      eeh_pe_traverse_func fn, void *flag);
void eeh_pe_dev_traverse(struct eeh_pe *root,
			 eeh_edev_traverse_func fn, void *flag);
void eeh_pe_restore_bars(struct eeh_pe *pe);
const char *eeh_pe_loc_get(struct eeh_pe *pe);
struct pci_bus *eeh_pe_bus_get(struct eeh_pe *pe);
void eeh_show_enabled(void);
int __init eeh_init(struct eeh_ops *ops);
int eeh_check_failure(const volatile void __iomem *token);
int eeh_dev_check_failure(struct eeh_dev *edev);
void eeh_addr_cache_init(void);
void eeh_probe_device(struct pci_dev *pdev);
void eeh_remove_device(struct pci_dev *);
int eeh_unfreeze_pe(struct eeh_pe *pe);
int eeh_pe_reset_and_recover(struct eeh_pe *pe);
int eeh_dev_open(struct pci_dev *pdev);
void eeh_dev_release(struct pci_dev *pdev);
struct eeh_pe *eeh_iommu_group_to_pe(struct iommu_group *group);
int eeh_pe_set_option(struct eeh_pe *pe, int option);
int eeh_pe_get_state(struct eeh_pe *pe);
int eeh_pe_reset(struct eeh_pe *pe, int option, bool include_passed);
int eeh_pe_configure(struct eeh_pe *pe);
int eeh_pe_inject_err(struct eeh_pe *pe, int type, int func,
		      unsigned long addr, unsigned long mask);
#define EEH_POSSIBLE_ERROR(val, type)	((val) == (type)~0 && eeh_enabled())
#define EEH_IO_ERROR_VALUE(size)	(~0U >> ((4 - (size)) * 8))
#else  
static inline bool eeh_enabled(void)
{
        return false;
}
static inline void eeh_show_enabled(void) { }
static inline int eeh_check_failure(const volatile void __iomem *token)
{
	return 0;
}
#define eeh_dev_check_failure(x) (0)
static inline void eeh_addr_cache_init(void) { }
static inline void eeh_probe_device(struct pci_dev *dev) { }
static inline void eeh_remove_device(struct pci_dev *dev) { }
#define EEH_POSSIBLE_ERROR(val, type) (0)
#define EEH_IO_ERROR_VALUE(size) (-1UL)
static inline int eeh_phb_pe_create(struct pci_controller *phb) { return 0; }
#endif  
#if defined(CONFIG_PPC_PSERIES) && defined(CONFIG_EEH)
void pseries_eeh_init_edev_recursive(struct pci_dn *pdn);
#endif
#ifdef CONFIG_PPC64
static inline u8 eeh_readb(const volatile void __iomem *addr)
{
	u8 val = in_8(addr);
	if (EEH_POSSIBLE_ERROR(val, u8))
		eeh_check_failure(addr);
	return val;
}
static inline u16 eeh_readw(const volatile void __iomem *addr)
{
	u16 val = in_le16(addr);
	if (EEH_POSSIBLE_ERROR(val, u16))
		eeh_check_failure(addr);
	return val;
}
static inline u32 eeh_readl(const volatile void __iomem *addr)
{
	u32 val = in_le32(addr);
	if (EEH_POSSIBLE_ERROR(val, u32))
		eeh_check_failure(addr);
	return val;
}
static inline u64 eeh_readq(const volatile void __iomem *addr)
{
	u64 val = in_le64(addr);
	if (EEH_POSSIBLE_ERROR(val, u64))
		eeh_check_failure(addr);
	return val;
}
static inline u16 eeh_readw_be(const volatile void __iomem *addr)
{
	u16 val = in_be16(addr);
	if (EEH_POSSIBLE_ERROR(val, u16))
		eeh_check_failure(addr);
	return val;
}
static inline u32 eeh_readl_be(const volatile void __iomem *addr)
{
	u32 val = in_be32(addr);
	if (EEH_POSSIBLE_ERROR(val, u32))
		eeh_check_failure(addr);
	return val;
}
static inline u64 eeh_readq_be(const volatile void __iomem *addr)
{
	u64 val = in_be64(addr);
	if (EEH_POSSIBLE_ERROR(val, u64))
		eeh_check_failure(addr);
	return val;
}
static inline void eeh_memcpy_fromio(void *dest, const
				     volatile void __iomem *src,
				     unsigned long n)
{
	_memcpy_fromio(dest, src, n);
	if (n >= 4 && EEH_POSSIBLE_ERROR(*((u32 *)(dest + n - 4)), u32))
		eeh_check_failure(src);
}
static inline void eeh_readsb(const volatile void __iomem *addr, void * buf,
			      int ns)
{
	_insb(addr, buf, ns);
	if (EEH_POSSIBLE_ERROR((*(((u8*)buf)+ns-1)), u8))
		eeh_check_failure(addr);
}
static inline void eeh_readsw(const volatile void __iomem *addr, void * buf,
			      int ns)
{
	_insw(addr, buf, ns);
	if (EEH_POSSIBLE_ERROR((*(((u16*)buf)+ns-1)), u16))
		eeh_check_failure(addr);
}
static inline void eeh_readsl(const volatile void __iomem *addr, void * buf,
			      int nl)
{
	_insl(addr, buf, nl);
	if (EEH_POSSIBLE_ERROR((*(((u32*)buf)+nl-1)), u32))
		eeh_check_failure(addr);
}
void __init eeh_cache_debugfs_init(void);
#endif  
#endif  
#endif  
