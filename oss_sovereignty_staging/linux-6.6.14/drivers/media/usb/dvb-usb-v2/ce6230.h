 
 

#ifndef CE6230_H
#define CE6230_H

#include "dvb_usb.h"
#include "zl10353.h"
#include "mxl5005s.h"

#define CE6230_USB_TIMEOUT 1000

struct usb_req {
	u8  cmd;        
	u16 value;      
	u16 index;      
	u16 data_len;   
	u8  *data;
};

enum ce6230_cmd {
	CONFIG_READ          = 0xd0,  
	UNKNOWN_WRITE        = 0xc7,  
	I2C_READ             = 0xd9,  
	I2C_WRITE            = 0xca,  
	DEMOD_READ           = 0xdb,  
	DEMOD_WRITE          = 0xcc,  
	REG_READ             = 0xde,  
	REG_WRITE            = 0xcf,  
};

#endif
