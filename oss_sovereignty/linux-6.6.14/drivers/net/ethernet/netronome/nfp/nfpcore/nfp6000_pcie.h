#ifndef NFP6000_PCIE_H
#define NFP6000_PCIE_H
#include "nfp_cpp.h"
struct nfp_cpp *
nfp_cpp_from_nfp6000_pcie(struct pci_dev *pdev, const struct nfp_dev_info *dev_info);
#endif  
