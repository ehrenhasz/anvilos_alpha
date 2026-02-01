 

#ifndef _EDAC_PCI_H_
#define _EDAC_PCI_H_

#include <linux/completion.h>
#include <linux/device.h>
#include <linux/edac.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <linux/workqueue.h>

#ifdef CONFIG_PCI

struct edac_pci_counter {
	atomic_t pe_count;
	atomic_t npe_count;
};

 
struct edac_pci_ctl_info {
	 
	struct list_head link;

	int pci_idx;

	struct bus_type *edac_subsys;	 

	 
	int op_state;
	 
	struct delayed_work work;

	 
	void (*edac_check) (struct edac_pci_ctl_info * edac_dev);

	struct device *dev;	 

	const char *mod_name;	 
	const char *ctl_name;	 
	const char *dev_name;	 

	void *pvt_info;		 

	unsigned long start_time;	 

	struct completion complete;

	 
	char name[EDAC_DEVICE_NAME_LEN + 1];

	 
	struct edac_pci_counter counters;

	 
	struct kobject kobj;
};

#define to_edac_pci_ctl_work(w) \
		container_of(w, struct edac_pci_ctl_info,work)

 
static inline void pci_write_bits8(struct pci_dev *pdev, int offset, u8 value,
				   u8 mask)
{
	if (mask != 0xff) {
		u8 buf;

		pci_read_config_byte(pdev, offset, &buf);
		value &= mask;
		buf &= ~mask;
		value |= buf;
	}

	pci_write_config_byte(pdev, offset, value);
}

 
static inline void pci_write_bits16(struct pci_dev *pdev, int offset,
				    u16 value, u16 mask)
{
	if (mask != 0xffff) {
		u16 buf;

		pci_read_config_word(pdev, offset, &buf);
		value &= mask;
		buf &= ~mask;
		value |= buf;
	}

	pci_write_config_word(pdev, offset, value);
}

 
static inline void pci_write_bits32(struct pci_dev *pdev, int offset,
				    u32 value, u32 mask)
{
	if (mask != 0xffffffff) {
		u32 buf;

		pci_read_config_dword(pdev, offset, &buf);
		value &= mask;
		buf &= ~mask;
		value |= buf;
	}

	pci_write_config_dword(pdev, offset, value);
}

#endif				 

 

 
extern struct edac_pci_ctl_info *edac_pci_alloc_ctl_info(unsigned int sz_pvt,
				const char *edac_pci_name);

 
extern void edac_pci_free_ctl_info(struct edac_pci_ctl_info *pci);

 
extern int edac_pci_alloc_index(void);

 
extern int edac_pci_add_device(struct edac_pci_ctl_info *pci, int edac_idx);

 
extern struct edac_pci_ctl_info *edac_pci_del_device(struct device *dev);

 
extern struct edac_pci_ctl_info *edac_pci_create_generic_ctl(
				struct device *dev,
				const char *mod_name);

 
extern void edac_pci_release_generic_ctl(struct edac_pci_ctl_info *pci);

 
extern int edac_pci_create_sysfs(struct edac_pci_ctl_info *pci);

 
extern void edac_pci_remove_sysfs(struct edac_pci_ctl_info *pci);

#endif
