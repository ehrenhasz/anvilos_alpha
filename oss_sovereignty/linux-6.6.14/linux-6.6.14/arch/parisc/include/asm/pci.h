#ifndef __ASM_PARISC_PCI_H
#define __ASM_PARISC_PCI_H
#include <linux/scatterlist.h>
#define PCI_MAX_BUSSES	256
#define pci_post_reset_delay 50
struct pci_hba_data {
	void __iomem   *base_addr;	 
	const struct parisc_device *dev;  
	struct pci_bus *hba_bus;	 
	int		hba_num;	 
	struct resource bus_num;	 
	struct resource io_space;	 
	struct resource lmmio_space;	 
	struct resource elmmio_space;	 
	struct resource gmmio_space;	 
	#define DINO_MAX_LMMIO_RESOURCES	3
	unsigned long   lmmio_space_offset;   
	struct ioc	*iommu;		 
	#define HBA_NAME_SIZE 16
	char io_name[HBA_NAME_SIZE];
	char lmmio_name[HBA_NAME_SIZE];
	char elmmio_name[HBA_NAME_SIZE];
	char gmmio_name[HBA_NAME_SIZE];
};
#define HBA_PORT_SPACE_BITS	16
#define HBA_PORT_BASE(h)	((h) << HBA_PORT_SPACE_BITS)
#define HBA_PORT_SPACE_SIZE	(1UL << HBA_PORT_SPACE_BITS)
#define PCI_PORT_HBA(a)		((a) >> HBA_PORT_SPACE_BITS)
#define PCI_PORT_ADDR(a)	((a) & (HBA_PORT_SPACE_SIZE - 1))
#ifdef CONFIG_64BIT
#define PCI_F_EXTEND		0xffffffff00000000UL
#else	 
#define PCI_F_EXTEND		0UL
#endif  
struct pci_port_ops {
	  u8 (*inb)  (struct pci_hba_data *hba, u16 port);
	 u16 (*inw)  (struct pci_hba_data *hba, u16 port);
	 u32 (*inl)  (struct pci_hba_data *hba, u16 port);
	void (*outb) (struct pci_hba_data *hba, u16 port,  u8 data);
	void (*outw) (struct pci_hba_data *hba, u16 port, u16 data);
	void (*outl) (struct pci_hba_data *hba, u16 port, u32 data);
};
struct pci_bios_ops {
	void (*init)(void);
	void (*fixup_bus)(struct pci_bus *bus);
};
extern struct pci_port_ops *pci_port;
extern struct pci_bios_ops *pci_bios;
#ifdef CONFIG_PCI
extern void pcibios_register_hba(struct pci_hba_data *);
#else
static inline void pcibios_register_hba(struct pci_hba_data *x)
{
}
#endif
extern void pcibios_init_bridge(struct pci_dev *);
#define pcibios_assign_all_busses()     (1)
#define PCIBIOS_MIN_IO          0x10
#define PCIBIOS_MIN_MEM         0x1000  
#define HAVE_PCI_MMAP
#define ARCH_GENERIC_PCI_MMAP_RESOURCE
#endif  
