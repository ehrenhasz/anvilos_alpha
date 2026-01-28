#ifndef MN88443X_H
#define MN88443X_H
#include <media/dvb_frontend.h>
#define DIRECT_IF_57MHZ    57000000
#define DIRECT_IF_44MHZ    44000000
#define LOW_IF_4MHZ        4000000
struct mn88443x_config {
	struct clk *mclk;
	u32 if_freq;
	struct gpio_desc *reset_gpio;
	struct dvb_frontend **fe;
};
#endif  
