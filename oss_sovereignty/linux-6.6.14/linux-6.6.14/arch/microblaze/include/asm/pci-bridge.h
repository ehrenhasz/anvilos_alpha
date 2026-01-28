#ifndef _ASM_MICROBLAZE_PCI_BRIDGE_H
#define _ASM_MICROBLAZE_PCI_BRIDGE_H
#ifdef __KERNEL__
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/ioport.h>
struct device_node;
#ifdef CONFIG_PCI
extern struct list_head hose_list;
extern int pcibios_vaddr_is_ioport(void __iomem *address);
#else
static inline int pcibios_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif
struct pci_controller {
	struct pci_bus *bus;
	struct list_head list_node;
	void __iomem *io_base_virt;
	struct resource io_resource;
};
#ifdef CONFIG_PCI
static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif  
#endif	 
#endif	 
