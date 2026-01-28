#ifndef _DVB_SI2165_H
#define _DVB_SI2165_H
#include <linux/dvb/frontend.h>
enum {
	SI2165_MODE_OFF = 0x00,
	SI2165_MODE_PLL_EXT = 0x20,
	SI2165_MODE_PLL_XTAL = 0x21
};
struct si2165_platform_data {
	struct dvb_frontend **fe;
	u8 chip_mode;
	u32 ref_freq_hz;
	bool inversion;
};
#endif  
