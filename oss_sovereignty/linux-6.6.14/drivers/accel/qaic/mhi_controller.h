 

#ifndef MHICONTROLLERQAIC_H_
#define MHICONTROLLERQAIC_H_

struct mhi_controller *qaic_mhi_register_controller(struct pci_dev *pci_dev, void __iomem *mhi_bar,
						    int mhi_irq);
void qaic_mhi_free_controller(struct mhi_controller *mhi_cntrl, bool link_up);
void qaic_mhi_start_reset(struct mhi_controller *mhi_cntrl);
void qaic_mhi_reset_done(struct mhi_controller *mhi_cntrl);

#endif  
