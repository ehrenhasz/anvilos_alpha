#ifndef _XTENSA_PCI_BRIDGE_H
#define _XTENSA_PCI_BRIDGE_H
struct device_node;
struct pci_controller;
extern int pciauto_bus_scan(struct pci_controller *, int);
struct pci_space {
	unsigned long start;
	unsigned long end;
	unsigned long base;
};
struct pci_controller {
	int index;			 
	struct pci_controller *next;
	struct pci_bus *bus;
	void *arch_data;
	int first_busno;
	int last_busno;
	struct pci_ops *ops;
	volatile unsigned int *cfg_addr;
	volatile unsigned char *cfg_data;
	struct resource	io_resource;
	struct resource mem_resources[3];
	int mem_resource_count;
	struct pci_space io_space;
	struct pci_space mem_space;
	int (*map_irq)(struct pci_dev*, u8, u8);
};
static inline void pcibios_init_resource(struct resource *res,
		unsigned long start, unsigned long end, int flags, char *name)
{
	res->start = start;
	res->end = end;
	res->flags = flags;
	res->name = name;
	res->parent = NULL;
	res->sibling = NULL;
	res->child = NULL;
}
#endif	 
