 
 

#ifndef __STI_THERMAL_SYSCFG_H
#define __STI_THERMAL_SYSCFG_H

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/thermal.h>

enum st_thermal_regfield_ids {
	INT_THRESH_HI = 0,  
	TEMP_PWR = 0,
	DCORRECT,
	OVERFLOW,
	DATA,
	INT_ENABLE,

	MAX_REGFIELDS
};

 
enum st_thermal_power_state {
	POWER_OFF = 0,
	POWER_ON
};

struct st_thermal_sensor;

 
struct st_thermal_sensor_ops {
	int (*power_ctrl)(struct st_thermal_sensor *, enum st_thermal_power_state);
	int (*alloc_regfields)(struct st_thermal_sensor *);
	int (*regmap_init)(struct st_thermal_sensor *);
	int (*register_enable_irq)(struct st_thermal_sensor *);
	int (*enable_irq)(struct st_thermal_sensor *);
};

 
struct st_thermal_compat_data {
	char *sys_compat;
	const struct reg_field *reg_fields;
	const struct st_thermal_sensor_ops *ops;
	unsigned int calibration_val;
	int temp_adjust_val;
	int crit_temp;
};

struct st_thermal_sensor {
	struct device *dev;
	struct thermal_zone_device *thermal_dev;
	const struct st_thermal_sensor_ops *ops;
	const struct st_thermal_compat_data *cdata;
	struct clk *clk;
	struct regmap *regmap;
	struct regmap_field *pwr;
	struct regmap_field *dcorrect;
	struct regmap_field *overflow;
	struct regmap_field *temp_data;
	struct regmap_field *int_thresh_hi;
	struct regmap_field *int_enable;
	int irq;
	void __iomem *mmio_base;
};

extern int st_thermal_register(struct platform_device *pdev,
			       const struct of_device_id *st_thermal_of_match);
extern void st_thermal_unregister(struct platform_device *pdev);
extern const struct dev_pm_ops st_thermal_pm_ops;

#endif  
