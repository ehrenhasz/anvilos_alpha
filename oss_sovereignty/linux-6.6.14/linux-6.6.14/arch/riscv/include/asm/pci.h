#ifndef _ASM_RISCV_PCI_H
#define _ASM_RISCV_PCI_H
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>
#define PCIBIOS_MIN_IO		4
#define PCIBIOS_MIN_MEM		16
#if defined(CONFIG_PCI) && defined(CONFIG_NUMA)
static inline int pcibus_to_node(struct pci_bus *bus)
{
	return dev_to_node(&bus->dev);
}
#ifndef cpumask_of_pcibus
#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))
#endif
#endif  
#include <asm-generic/pci.h>
#endif   
