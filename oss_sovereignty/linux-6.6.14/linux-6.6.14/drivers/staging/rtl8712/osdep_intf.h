#ifndef __OSDEP_INTF_H_
#define __OSDEP_INTF_H_
#include "osdep_service.h"
#include "drv_types.h"
#define RND4(x)	(((x >> 2) + ((x & 3) != 0)) << 2)
struct intf_priv {
	u8 *intf_dev;
	struct usb_device *udev;
	struct urb *piorw_urb;
	struct completion io_retevt_comp;
};
int r871x_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
#endif	 
