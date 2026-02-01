 
 

#ifndef CX24120_H
#define CX24120_H

#include <linux/dvb/frontend.h>
#include <linux/firmware.h>

struct cx24120_initial_mpeg_config {
	u8 x1;
	u8 x2;
	u8 x3;
};

struct cx24120_config {
	u8 i2c_addr;
	u32 xtal_khz;
	struct cx24120_initial_mpeg_config initial_mpeg_config;

	int (*request_firmware)(struct dvb_frontend *fe,
				const struct firmware **fw, char *name);

	 
	u16 i2c_wr_max;
};

#if IS_REACHABLE(CONFIG_DVB_CX24120)
struct dvb_frontend *cx24120_attach(const struct cx24120_config *config,
				    struct i2c_adapter *i2c);
#else
static inline
struct dvb_frontend *cx24120_attach(const struct cx24120_config *config,
				    struct i2c_adapter *i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif

#endif  
