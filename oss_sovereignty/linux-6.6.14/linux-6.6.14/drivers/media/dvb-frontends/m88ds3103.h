#ifndef M88DS3103_H
#define M88DS3103_H
#include <linux/dvb/frontend.h>
enum m88ds3103_ts_mode {
	M88DS3103_TS_SERIAL,
	M88DS3103_TS_SERIAL_D7,
	M88DS3103_TS_PARALLEL,
	M88DS3103_TS_CI
};
enum m88ds3103_clock_out {
	M88DS3103_CLOCK_OUT_DISABLED,
	M88DS3103_CLOCK_OUT_ENABLED,
	M88DS3103_CLOCK_OUT_ENABLED_DIV2
};
struct m88ds3103_platform_data {
	u32 clk;
	u16 i2c_wr_max;
	enum m88ds3103_ts_mode ts_mode;
	u32 ts_clk;
	enum m88ds3103_clock_out clk_out;
	u8 ts_clk_pol:1;
	u8 spec_inv:1;
	u8 agc;
	u8 agc_inv:1;
	u8 envelope_mode:1;
	u8 lnb_hv_pol:1;
	u8 lnb_en_pol:1;
	struct dvb_frontend* (*get_dvb_frontend)(struct i2c_client *);
	struct i2c_adapter* (*get_i2c_adapter)(struct i2c_client *);
	u8 attach_in_use:1;
};
struct m88ds3103_config {
	u8 i2c_addr;
	u32 clock;
	u16 i2c_wr_max;
	u8 ts_mode;
	u32 ts_clk;
	u8 ts_clk_pol:1;
	u8 spec_inv:1;
	u8 agc_inv:1;
	u8 clock_out;
	u8 envelope_mode:1;
	u8 agc;
	u8 lnb_hv_pol:1;
	u8 lnb_en_pol:1;
};
#if defined(CONFIG_DVB_M88DS3103) || \
		(defined(CONFIG_DVB_M88DS3103_MODULE) && defined(MODULE))
extern struct dvb_frontend *m88ds3103_attach(
		const struct m88ds3103_config *config,
		struct i2c_adapter *i2c,
		struct i2c_adapter **tuner_i2c);
extern int m88ds3103_get_agc_pwm(struct dvb_frontend *fe, u8 *_agc_pwm);
#else
static inline struct dvb_frontend *m88ds3103_attach(
		const struct m88ds3103_config *config,
		struct i2c_adapter *i2c,
		struct i2c_adapter **tuner_i2c)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#define m88ds3103_get_agc_pwm NULL
#endif
#endif
