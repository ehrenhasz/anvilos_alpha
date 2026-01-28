


#ifndef __DRIVER_USB_CHIPIDEA_CI_HDRC_IMX_H
#define __DRIVER_USB_CHIPIDEA_CI_HDRC_IMX_H

struct imx_usbmisc_data {
	struct device *dev;
	int index;

	unsigned int disable_oc:1; 

	
	unsigned int oc_pol_active_low:1;

	
	unsigned int oc_pol_configured:1;

	unsigned int pwr_pol:1; 
	unsigned int evdo:1; 
	unsigned int ulpi:1; 
	unsigned int hsic:1; 
	unsigned int ext_id:1; 
	unsigned int ext_vbus:1; 
	struct usb_phy *usb_phy;
	enum usb_dr_mode available_role; 
	int emp_curr_control;
	int dc_vol_level_adjust;
	int rise_fall_time_adjust;
};

int imx_usbmisc_init(struct imx_usbmisc_data *data);
int imx_usbmisc_init_post(struct imx_usbmisc_data *data);
int imx_usbmisc_hsic_set_connect(struct imx_usbmisc_data *data);
int imx_usbmisc_charger_detection(struct imx_usbmisc_data *data, bool connect);
int imx_usbmisc_suspend(struct imx_usbmisc_data *data, bool wakeup);
int imx_usbmisc_resume(struct imx_usbmisc_data *data, bool wakeup);

#endif 
