 
 

#ifndef USB6FIRE_FIRMWARE_H
#define USB6FIRE_FIRMWARE_H

#include "common.h"

enum  
{
	FW_READY = 0,
	FW_NOT_READY = 1
};

int usb6fire_fw_init(struct usb_interface *intf);
#endif  

