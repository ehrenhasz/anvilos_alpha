#ifndef __ASM_MICROBLAZE_PCI_H
#define __ASM_MICROBLAZE_PCI_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/pci.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <asm/pci-bridge.h>
#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000
#define pcibios_assign_all_busses()	0
extern int pci_domain_nr(struct pci_bus *bus);
extern int pci_proc_domain(struct pci_bus *bus);
#define HAVE_PCI_MMAP			1
#define ARCH_GENERIC_PCI_MMAP_RESOURCE	1
struct file;
static inline void __init xilinx_pci_init(void) { return; }
#endif	 
#endif  
