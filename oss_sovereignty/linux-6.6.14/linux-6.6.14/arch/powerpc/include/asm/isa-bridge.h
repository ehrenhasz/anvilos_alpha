#ifndef __ISA_BRIDGE_H
#define __ISA_BRIDGE_H
#ifdef CONFIG_PPC64
extern void isa_bridge_find_early(struct pci_controller *hose);
extern void isa_bridge_init_non_pci(struct device_node *np);
static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	unsigned long ea = (unsigned long)address;
	return ea >= ISA_IO_BASE && ea < ISA_IO_END;
}
#else
static inline int isa_vaddr_is_ioport(void __iomem *address)
{
	return 0;
}
#endif
#endif  
