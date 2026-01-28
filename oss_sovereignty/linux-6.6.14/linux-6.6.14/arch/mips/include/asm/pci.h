#ifndef _ASM_PCI_H
#define _ASM_PCI_H
#include <linux/mm.h>
#ifdef __KERNEL__
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/of.h>
#ifdef CONFIG_PCI_DRIVERS_LEGACY
struct pci_controller {
	struct list_head list;
	struct pci_bus *bus;
	struct device_node *of_node;
	struct pci_ops *pci_ops;
	struct resource *mem_resource;
	unsigned long mem_offset;
	struct resource *io_resource;
	unsigned long io_offset;
	unsigned long io_map_base;
#ifndef CONFIG_PCI_DOMAINS_GENERIC
	unsigned int index;
	unsigned int need_domain_info;
#endif
	int (*get_busno)(void);
	void (*set_busno)(int busno);
};
extern void register_pci_controller(struct pci_controller *hose);
extern int pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin);
extern int pcibios_plat_dev_init(struct pci_dev *dev);
extern char * (*pcibios_plat_setup)(char *str);
#ifdef CONFIG_OF
extern void pci_load_of_ranges(struct pci_controller *hose,
			       struct device_node *node);
#else
static inline void pci_load_of_ranges(struct pci_controller *hose,
				      struct device_node *node) {}
#endif
#ifdef CONFIG_PCI_DOMAINS_GENERIC
static inline void set_pci_need_domain_info(struct pci_controller *hose,
					    int need_domain_info)
{
}
#elif defined(CONFIG_PCI_DOMAINS)
static inline void set_pci_need_domain_info(struct pci_controller *hose,
					    int need_domain_info)
{
	hose->need_domain_info = need_domain_info;
}
#endif  
#endif
static inline unsigned int pcibios_assign_all_busses(void)
{
	return 1;
}
extern unsigned long PCIBIOS_MIN_IO;
extern unsigned long PCIBIOS_MIN_MEM;
#define PCIBIOS_MIN_CARDBUS_IO	0x4000
#define HAVE_PCI_MMAP
#define ARCH_GENERIC_PCI_MMAP_RESOURCE
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/string.h>
#include <asm/io.h>
#ifdef CONFIG_PCI_DOMAINS_GENERIC
static inline int pci_proc_domain(struct pci_bus *bus)
{
	return pci_domain_nr(bus);
}
#elif defined(CONFIG_PCI_DOMAINS)
#define pci_domain_nr(bus) ((struct pci_controller *)(bus)->sysdata)->index
static inline int pci_proc_domain(struct pci_bus *bus)
{
	struct pci_controller *hose = bus->sysdata;
	return hose->need_domain_info;
}
#endif  
#endif  
extern int pcibios_plat_dev_init(struct pci_dev *dev);
#endif  
