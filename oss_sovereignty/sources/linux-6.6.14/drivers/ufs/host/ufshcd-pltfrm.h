


#ifndef UFSHCD_PLTFRM_H_
#define UFSHCD_PLTFRM_H_

#include <ufs/ufshcd.h>

#define UFS_PWM_MODE 1
#define UFS_HS_MODE  2

struct ufs_dev_params {
	u32 pwm_rx_gear;        
	u32 pwm_tx_gear;        
	u32 hs_rx_gear;         
	u32 hs_tx_gear;         
	u32 rx_lanes;           
	u32 tx_lanes;           
	u32 rx_pwr_pwm;         
	u32 tx_pwr_pwm;         
	u32 rx_pwr_hs;          
	u32 tx_pwr_hs;          
	u32 hs_rate;            
	u32 desired_working_mode;
};

int ufshcd_get_pwr_dev_param(const struct ufs_dev_params *dev_param,
			     const struct ufs_pa_layer_attr *dev_max,
			     struct ufs_pa_layer_attr *agreed_pwr);
void ufshcd_init_pwr_dev_param(struct ufs_dev_params *dev_param);
int ufshcd_pltfrm_init(struct platform_device *pdev,
		       const struct ufs_hba_variant_ops *vops);
int ufshcd_populate_vreg(struct device *dev, const char *name,
			 struct ufs_vreg **out_vreg);

#endif 
