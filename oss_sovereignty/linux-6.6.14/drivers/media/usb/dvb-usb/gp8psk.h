 
 
#ifndef _DVB_USB_GP8PSK_H_
#define _DVB_USB_GP8PSK_H_

#define DVB_USB_LOG_PREFIX "gp8psk"
#include "dvb-usb.h"

extern int dvb_usb_gp8psk_debug;
#define deb_info(args...) dprintk(dvb_usb_gp8psk_debug,0x01,args)
#define deb_xfer(args...) dprintk(dvb_usb_gp8psk_debug,0x02,args)
#define deb_rc(args...)   dprintk(dvb_usb_gp8psk_debug,0x04,args)

#define GET_USB_SPEED                     0x07

#define RESET_FX2                         0x13

#define FW_VERSION_READ                   0x0B
#define VENDOR_STRING_READ                0x0C
#define PRODUCT_STRING_READ               0x0D
#define FW_BCD_VERSION_READ               0x14

#endif
