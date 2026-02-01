 
 

 

#ifndef RT2X00PCI_H
#define RT2X00PCI_H

#include <linux/io.h>
#include <linux/pci.h>

 
int rt2x00pci_probe(struct pci_dev *pci_dev, const struct rt2x00_ops *ops);
void rt2x00pci_remove(struct pci_dev *pci_dev);

extern const struct dev_pm_ops rt2x00pci_pm_ops;

#endif  
