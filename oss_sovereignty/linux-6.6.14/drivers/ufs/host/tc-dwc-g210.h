 
 

#ifndef _TC_DWC_G210_H
#define _TC_DWC_G210_H

struct ufs_hba;

int tc_dwc_g210_config_40_bit(struct ufs_hba *hba);
int tc_dwc_g210_config_20_bit(struct ufs_hba *hba);

#endif  
