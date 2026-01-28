#ifndef STV0299_H
#define STV0299_H
#include <linux/dvb/frontend.h>
#include <media/dvb_frontend.h>
#define STV0299_LOCKOUTPUT_0  0
#define STV0299_LOCKOUTPUT_1  1
#define STV0299_LOCKOUTPUT_CF 2
#define STV0299_LOCKOUTPUT_LK 3
#define STV0299_VOLT13_OP0 0
#define STV0299_VOLT13_OP1 1
struct stv0299_config
{
	u8 demod_address;
	const u8* inittab;
	u32 mclk;
	u8 invert:1;
	u8 skip_reinit:1;
	u8 lock_output:2;
	u8 volt13_op0_op1:1;
	u8 op0_off:1;
	int min_delay_ms;
	int (*set_symbol_rate)(struct dvb_frontend *fe, u32 srate, u32 ratio);
	int (*set_ts_params)(struct dvb_frontend *fe, int is_punctured);
};
#if IS_REACHABLE(CONFIG_DVB_STV0299)
extern struct dvb_frontend *stv0299_attach(const struct stv0299_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *stv0299_attach(const struct stv0299_config *config,
					   struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  
static inline int stv0299_writereg(struct dvb_frontend *fe, u8 reg, u8 val) {
	int r = 0;
	u8 buf[] = {reg, val};
	if (fe->ops.write)
		r = fe->ops.write(fe, buf, 2);
	return r;
}
#endif  
