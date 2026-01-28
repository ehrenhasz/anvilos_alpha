#ifndef R820T_H
#define R820T_H
#include <media/dvb_frontend.h>
enum r820t_chip {
	CHIP_R820T,
	CHIP_R620D,
	CHIP_R828D,
	CHIP_R828,
	CHIP_R828S,
	CHIP_R820C,
};
struct r820t_config {
	u8 i2c_addr;		 
	u32 xtal;
	enum r820t_chip rafael_chip;
	unsigned max_i2c_msg_len;
	bool use_diplexer;
	bool use_predetect;
};
#if IS_REACHABLE(CONFIG_MEDIA_TUNER_R820T)
struct dvb_frontend *r820t_attach(struct dvb_frontend *fe,
				  struct i2c_adapter *i2c,
				  const struct r820t_config *cfg);
#else
static inline struct dvb_frontend *r820t_attach(struct dvb_frontend *fe,
						struct i2c_adapter *i2c,
						const struct r820t_config *cfg)
{
	pr_warn("%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif
#endif
