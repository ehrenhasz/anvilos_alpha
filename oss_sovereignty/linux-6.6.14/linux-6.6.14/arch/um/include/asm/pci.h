#ifndef __ASM_UM_PCI_H
#define __ASM_UM_PCI_H
#include <linux/types.h>
#include <asm/io.h>
#include <asm-generic/pci.h>
#ifdef CONFIG_PCI_MSI
void *pci_root_bus_fwnode(struct pci_bus *bus);
#define pci_root_bus_fwnode	pci_root_bus_fwnode
#endif
#endif   
