#ifndef STV0900_H
#define STV0900_H
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
struct stv0900_reg {
	u16 addr;
	u8  val;
};
struct stv0900_config {
	u8 demod_address;
	u8 demod_mode;
	u32 xtal;
	u8 clkmode; 
	u8 diseqc_mode;
	u8 path1_mode;
	u8 path2_mode;
	struct stv0900_reg *ts_config_regs;
	u8 tun1_maddress; 
	u8 tun2_maddress;
	u8 tun1_adc; 
	u8 tun2_adc;
	u8 tun1_type; 
	u8 tun2_type;
	int (*set_ts_params)(struct dvb_frontend *fe, int is_punctured);
	void (*set_lock_led)(struct dvb_frontend *fe, int offon);
};
#if IS_REACHABLE(CONFIG_DVB_STV0900)
extern struct dvb_frontend *stv0900_attach(const struct stv0900_config *config,
					struct i2c_adapter *i2c, int demod);
#else
static inline struct dvb_frontend *stv0900_attach(const struct stv0900_config *config,
					struct i2c_adapter *i2c, int demod)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif
