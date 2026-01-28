#ifndef __ASM_MACH_LOONGSON64_PCI_H_
#define __ASM_MACH_LOONGSON64_PCI_H_
extern struct pci_ops loongson_pci_ops;
#define LOONGSON_PCI_IO_START	0x00004000UL
#define LOONGSON_PCI_MEM_START	0x40000000UL
#define LOONGSON_PCI_MEM_END	0x7effffffUL
#endif  
