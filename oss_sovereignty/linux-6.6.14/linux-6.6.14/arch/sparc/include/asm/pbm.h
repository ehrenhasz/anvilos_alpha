#ifndef __SPARC_PBM_H
#define __SPARC_PBM_H
#include <linux/pci.h>
#include <asm/oplib.h>
#include <asm/prom.h>
struct linux_pbm_info {
	int		prom_node;
	char		prom_name[64];
	unsigned int	pci_first_busno;	 
	struct pci_bus	*pci_bus;		 
};
struct pcidev_cookie {
	struct linux_pbm_info		*pbm;
	struct device_node		*prom_node;
};
#endif  
