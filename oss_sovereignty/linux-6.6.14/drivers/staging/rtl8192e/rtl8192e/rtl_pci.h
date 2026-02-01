 
 
#ifndef _RTL_PCI_H
#define _RTL_PCI_H

#include <linux/types.h>
#include <linux/pci.h>

struct net_device;
bool rtl92e_check_adapter(struct pci_dev *pdev, struct net_device *dev);

#endif
