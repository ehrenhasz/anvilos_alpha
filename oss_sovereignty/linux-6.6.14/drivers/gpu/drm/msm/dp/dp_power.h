 
 

#ifndef _DP_POWER_H_
#define _DP_POWER_H_

#include "dp_parser.h"

 
struct dp_power {
	bool core_clks_on;
	bool link_clks_on;
	bool stream_clks_on;
};

 
int dp_power_init(struct dp_power *power);

 
int dp_power_deinit(struct dp_power *power);

 

int dp_power_clk_status(struct dp_power *dp_power, enum dp_pm_type pm_type);

 

int dp_power_clk_enable(struct dp_power *power, enum dp_pm_type pm_type,
				bool enable);

 
int dp_power_client_init(struct dp_power *power);

 
void dp_power_client_deinit(struct dp_power *power);

 
struct dp_power *dp_power_get(struct device *dev, struct dp_parser *parser);

#endif  
