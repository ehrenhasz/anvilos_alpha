#ifndef _ASM_LEON_PCI_H_
#define _ASM_LEON_PCI_H_
struct leon_pci_info {
	struct pci_ops *ops;
	struct resource	io_space;
	struct resource	mem_space;
	struct resource	busn;
	int (*map_irq)(const struct pci_dev *dev, u8 slot, u8 pin);
};
void leon_pci_init(struct platform_device *ofdev,
		   struct leon_pci_info *info);
#endif  
