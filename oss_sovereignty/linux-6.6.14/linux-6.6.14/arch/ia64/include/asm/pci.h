#ifndef _ASM_IA64_PCI_H
#define _ASM_IA64_PCI_H
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/scatterlist.h>
#include <asm/io.h>
#include <asm/hw_irq.h>
struct pci_vector_struct {
	__u16 segment;	 
	__u16 bus;	 
	__u32 pci_id;	 
	__u8 pin;	 
	__u32 irq;	 
};
#define pcibios_assign_all_busses()     0
#define PCIBIOS_MIN_IO		0x1000
#define PCIBIOS_MIN_MEM		0x10000000
#define HAVE_PCI_MMAP
#define ARCH_GENERIC_PCI_MMAP_RESOURCE
#define arch_can_pci_mmap_wc()	1
#define HAVE_PCI_LEGACY
extern int pci_mmap_legacy_page_range(struct pci_bus *bus,
				      struct vm_area_struct *vma,
				      enum pci_mmap_state mmap_state);
char *pci_get_legacy_mem(struct pci_bus *bus);
int pci_legacy_read(struct pci_bus *bus, u16 port, u32 *val, u8 size);
int pci_legacy_write(struct pci_bus *bus, u16 port, u32 val, u8 size);
struct pci_controller {
	struct acpi_device *companion;
	void *iommu;
	int segment;
	int node;		 
	void *platform_data;
};
#define PCI_CONTROLLER(busdev) ((struct pci_controller *) busdev->sysdata)
#define pci_domain_nr(busdev)    (PCI_CONTROLLER(busdev)->segment)
extern struct pci_ops pci_root_ops;
static inline int pci_proc_domain(struct pci_bus *bus)
{
	return (pci_domain_nr(bus) != 0);
}
#endif  
