#ifndef __PLAT_SAMSUNG_USB_PHY_H
#define __PLAT_SAMSUNG_USB_PHY_H __FILE__
extern int s3c_usb_phy_init(struct platform_device *pdev, int type);
extern int s3c_usb_phy_exit(struct platform_device *pdev, int type);
#endif  
