 
 

#ifndef __S5H1409_H__
#define __S5H1409_H__

#include <linux/dvb/frontend.h>

struct s5h1409_config {
	 
	u8 demod_address;

	 
#define S5H1409_PARALLEL_OUTPUT 0
#define S5H1409_SERIAL_OUTPUT   1
	u8 output_mode;

	 
#define S5H1409_GPIO_OFF 0
#define S5H1409_GPIO_ON  1
	u8 gpio;

	 
	u16 qam_if;

	 
#define S5H1409_INVERSION_OFF 0
#define S5H1409_INVERSION_ON  1
	u8 inversion;

	 
#define S5H1409_TUNERLOCKING 0
#define S5H1409_DEMODLOCKING 1
	u8 status_mode;

	 
#define S5H1409_MPEGTIMING_CONTINUOUS_INVERTING_CLOCK       0
#define S5H1409_MPEGTIMING_CONTINUOUS_NONINVERTING_CLOCK    1
#define S5H1409_MPEGTIMING_NONCONTINUOUS_INVERTING_CLOCK    2
#define S5H1409_MPEGTIMING_NONCONTINUOUS_NONINVERTING_CLOCK 3
	u16 mpeg_timing;

	 
#define S5H1409_HVR1600_NOOPTIMIZE 0
#define S5H1409_HVR1600_OPTIMIZE   1
	u8 hvr1600_opt;
};

#if IS_REACHABLE(CONFIG_DVB_S5H1409)
extern struct dvb_frontend *s5h1409_attach(const struct s5h1409_config *config,
					   struct i2c_adapter *i2c);
#else
static inline struct dvb_frontend *s5h1409_attach(
	const struct s5h1409_config *config,
	struct i2c_adapter *i2c)
{
	printk(KERN_WARNING "%s: driver disabled by Kconfig\n", __func__);
	return NULL;
}
#endif  

#endif  
