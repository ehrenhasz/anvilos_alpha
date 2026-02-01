 
 

#ifndef _AB8500_CHARGALG_H_
#define _AB8500_CHARGALG_H_

#include <linux/power_supply.h>

 
#define psy_to_ux500_charger(x) power_supply_get_drvdata(x)

 
struct ux500_charger;

struct ux500_charger_ops {
	int (*enable) (struct ux500_charger *, int, int, int);
	int (*check_enable) (struct ux500_charger *, int, int);
	int (*kick_wd) (struct ux500_charger *);
	int (*update_curr) (struct ux500_charger *, int);
};

 
struct ux500_charger {
	struct power_supply *psy;
	struct ux500_charger_ops ops;
	int max_out_volt_uv;
	int max_out_curr_ua;
	int wdt_refresh;
	bool enabled;
};

#endif  
