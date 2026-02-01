 
 
#ifndef __STI_RESET_SYSCFG_H
#define __STI_RESET_SYSCFG_H

#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/reset-controller.h>

 
struct syscfg_reset_channel_data {
	const char *compatible;
	struct reg_field reset;
	struct reg_field ack;
};

#define _SYSCFG_RST_CH(_c, _rr, _rb, _ar, _ab)		\
	{ .compatible	= _c,				\
	  .reset	= REG_FIELD(_rr, _rb, _rb),	\
	  .ack		= REG_FIELD(_ar, _ab, _ab), }

#define _SYSCFG_RST_CH_NO_ACK(_c, _rr, _rb)		\
	{ .compatible	= _c,			\
	  .reset	= REG_FIELD(_rr, _rb, _rb), }

 
struct syscfg_reset_controller_data {
	bool wait_for_ack;
	bool active_low;
	int nr_channels;
	const struct syscfg_reset_channel_data *channels;
};

 
int syscfg_reset_probe(struct platform_device *pdev);

#endif  
