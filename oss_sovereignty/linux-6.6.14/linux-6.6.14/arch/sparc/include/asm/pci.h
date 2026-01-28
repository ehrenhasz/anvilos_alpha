#ifndef ___ASM_SPARC_PCI_H
#define ___ASM_SPARC_PCI_H
#define pcibios_assign_all_busses()	0
#define PCIBIOS_MIN_IO		0UL
#define PCIBIOS_MIN_MEM		0UL
#define PCI_IRQ_NONE		0xffffffff
#ifdef CONFIG_SPARC64
#define PCI64_REQUIRED_MASK	(~(u64)0)
#define PCI64_ADDR_BASE		0xfffc000000000000UL
int pci_domain_nr(struct pci_bus *bus);
static inline int pci_proc_domain(struct pci_bus *bus)
{
	return 1;
}
#define HAVE_PCI_MMAP
#define arch_can_pci_mmap_io()	1
#define HAVE_ARCH_PCI_GET_UNMAPPED_AREA
#define ARCH_GENERIC_PCI_MMAP_RESOURCE
#define get_pci_unmapped_area get_fb_unmapped_area
#endif  
#endif  
