#ifndef _DVB_USB_ANYSEE_H_
#define _DVB_USB_ANYSEE_H_
#define DVB_USB_LOG_PREFIX "anysee"
#include "dvb_usb.h"
#include <media/dvb_ca_en50221.h>
enum cmd {
	CMD_I2C_READ            = 0x33,
	CMD_I2C_WRITE           = 0x31,
	CMD_REG_READ            = 0xb0,
	CMD_REG_WRITE           = 0xb1,
	CMD_STREAMING_CTRL      = 0x12,
	CMD_LED_AND_IR_CTRL     = 0x16,
	CMD_GET_IR_CODE         = 0x41,
	CMD_GET_HW_INFO         = 0x19,
	CMD_SMARTCARD           = 0x34,
	CMD_CI                  = 0x37,
};
struct anysee_state {
	u8 buf[64];
	u8 seq;
	u8 hw;  
	#define ANYSEE_I2C_CLIENT_MAX 1
	struct i2c_client *i2c_client[ANYSEE_I2C_CLIENT_MAX];
	u8 fe_id:1;  
	u8 has_ci:1;
	u8 has_tda18212:1;
	u8 ci_attached:1;
	struct dvb_ca_en50221 ci;
	unsigned long ci_cam_ready;  
};
#define ANYSEE_HW_507T    2  
#define ANYSEE_HW_507CD   6  
#define ANYSEE_HW_507DC  10  
#define ANYSEE_HW_507SI  11  
#define ANYSEE_HW_507FA  15  
#define ANYSEE_HW_508TC  18  
#define ANYSEE_HW_508S2  19  
#define ANYSEE_HW_508T2C 20  
#define ANYSEE_HW_508PTC 21  
#define ANYSEE_HW_508PS2 22  
#define REG_IOA       0x80  
#define REG_IOB       0x90  
#define REG_IOC       0xa0  
#define REG_IOD       0xb0  
#define REG_IOE       0xb1  
#define REG_OEA       0xb2  
#define REG_OEB       0xb3  
#define REG_OEC       0xb4  
#define REG_OED       0xb5  
#define REG_OEE       0xb6  
#endif
