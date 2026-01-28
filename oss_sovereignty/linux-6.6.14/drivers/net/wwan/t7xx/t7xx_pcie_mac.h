#ifndef __T7XX_PCIE_MAC_H__
#define __T7XX_PCIE_MAC_H__
#include "t7xx_pci.h"
#include "t7xx_reg.h"
#define IREG_BASE(t7xx_dev)	((t7xx_dev)->base_addr.pcie_mac_ireg_base)
void t7xx_pcie_mac_interrupts_en(struct t7xx_pci_dev *t7xx_dev);
void t7xx_pcie_mac_interrupts_dis(struct t7xx_pci_dev *t7xx_dev);
void t7xx_pcie_mac_atr_init(struct t7xx_pci_dev *t7xx_dev);
void t7xx_pcie_mac_clear_int(struct t7xx_pci_dev *t7xx_dev, enum t7xx_int int_type);
void t7xx_pcie_mac_set_int(struct t7xx_pci_dev *t7xx_dev, enum t7xx_int int_type);
void t7xx_pcie_mac_clear_int_status(struct t7xx_pci_dev *t7xx_dev, enum t7xx_int int_type);
void t7xx_pcie_set_mac_msix_cfg(struct t7xx_pci_dev *t7xx_dev, unsigned int irq_count);
#endif  
