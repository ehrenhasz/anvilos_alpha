 
 
#ifndef __TI_BANDGAP_H
#define __TI_BANDGAP_H

#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/cpu_pm.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/pm.h>

struct gpio_desc;

 

 

struct temp_sensor_registers {
	u32	temp_sensor_ctrl;
	u32	bgap_tempsoff_mask;
	u32	bgap_soc_mask;
	u32	bgap_eocz_mask;
	u32	bgap_dtemp_mask;

	u32	bgap_mask_ctrl;
	u32	mask_hot_mask;
	u32	mask_cold_mask;
	u32	mask_counter_delay_mask;
	u32	mask_freeze_mask;

	u32	bgap_mode_ctrl;
	u32	mode_ctrl_mask;

	u32	bgap_counter;
	u32	counter_mask;

	u32	bgap_threshold;
	u32	threshold_thot_mask;
	u32	threshold_tcold_mask;

	u32	tshut_threshold;
	u32	tshut_hot_mask;
	u32	tshut_cold_mask;

	u32	bgap_status;
	u32	status_hot_mask;
	u32	status_cold_mask;

	u32	ctrl_dtemp_1;
	u32	ctrl_dtemp_2;
	u32	bgap_efuse;
};

 
struct temp_sensor_data {
	u32	tshut_hot;
	u32	tshut_cold;
	u32	t_hot;
	u32	t_cold;
	u32	min_freq;
	u32	max_freq;
};

struct ti_bandgap_data;

 
struct temp_sensor_regval {
	u32			bg_mode_ctrl;
	u32			bg_ctrl;
	u32			bg_counter;
	u32			bg_threshold;
	u32			tshut_threshold;
	void			*data;
};

 
struct ti_bandgap {
	struct device			*dev;
	void __iomem			*base;
	const struct ti_bandgap_data	*conf;
	struct temp_sensor_regval	*regval;
	struct clk			*fclock;
	struct clk			*div_clk;
	spinlock_t			lock;  
	int				irq;
	struct gpio_desc		*tshut_gpiod;
	u32				clk_rate;
	struct notifier_block		nb;
	unsigned int is_suspended:1;
};

 
struct ti_temp_sensor {
	struct temp_sensor_data		*ts_data;
	struct temp_sensor_registers	*registers;
	char				*domain;
	 
	const int			slope_pcb;
	const int			constant_pcb;
	int (*register_cooling)(struct ti_bandgap *bgp, int id);
	int (*unregister_cooling)(struct ti_bandgap *bgp, int id);
};

 
#define TI_BANDGAP_FEATURE_TSHUT		BIT(0)
#define TI_BANDGAP_FEATURE_TSHUT_CONFIG		BIT(1)
#define TI_BANDGAP_FEATURE_TALERT		BIT(2)
#define TI_BANDGAP_FEATURE_MODE_CONFIG		BIT(3)
#define TI_BANDGAP_FEATURE_COUNTER		BIT(4)
#define TI_BANDGAP_FEATURE_POWER_SWITCH		BIT(5)
#define TI_BANDGAP_FEATURE_CLK_CTRL		BIT(6)
#define TI_BANDGAP_FEATURE_FREEZE_BIT		BIT(7)
#define TI_BANDGAP_FEATURE_COUNTER_DELAY	BIT(8)
#define TI_BANDGAP_FEATURE_HISTORY_BUFFER	BIT(9)
#define TI_BANDGAP_FEATURE_ERRATA_814		BIT(10)
#define TI_BANDGAP_FEATURE_UNRELIABLE		BIT(11)
#define TI_BANDGAP_FEATURE_CONT_MODE_ONLY	BIT(12)
#define TI_BANDGAP_HAS(b, f)			\
			((b)->conf->features & TI_BANDGAP_FEATURE_ ## f)

 
struct ti_bandgap_data {
	unsigned int			features;
	const int			*conv_table;
	u32				adc_start_val;
	u32				adc_end_val;
	char				*fclock_name;
	char				*div_ck_name;
	int				sensor_count;
	int (*report_temperature)(struct ti_bandgap *bgp, int id);
	int (*expose_sensor)(struct ti_bandgap *bgp, int id, char *domain);
	int (*remove_sensor)(struct ti_bandgap *bgp, int id);

	 
	struct ti_temp_sensor		sensors[];
};

int ti_bandgap_read_thot(struct ti_bandgap *bgp, int id, int *thot);
int ti_bandgap_write_thot(struct ti_bandgap *bgp, int id, int val);
int ti_bandgap_read_tcold(struct ti_bandgap *bgp, int id, int *tcold);
int ti_bandgap_write_tcold(struct ti_bandgap *bgp, int id, int val);
int ti_bandgap_read_update_interval(struct ti_bandgap *bgp, int id,
				    int *interval);
int ti_bandgap_write_update_interval(struct ti_bandgap *bgp, int id,
				     u32 interval);
int ti_bandgap_read_temperature(struct ti_bandgap *bgp, int id,
				  int *temperature);
int ti_bandgap_set_sensor_data(struct ti_bandgap *bgp, int id, void *data);
void *ti_bandgap_get_sensor_data(struct ti_bandgap *bgp, int id);
int ti_bandgap_get_trend(struct ti_bandgap *bgp, int id, int *trend);

#ifdef CONFIG_OMAP3_THERMAL
extern const struct ti_bandgap_data omap34xx_data;
extern const struct ti_bandgap_data omap36xx_data;
#else
#define omap34xx_data					NULL
#define omap36xx_data					NULL
#endif

#ifdef CONFIG_OMAP4_THERMAL
extern const struct ti_bandgap_data omap4430_data;
extern const struct ti_bandgap_data omap4460_data;
extern const struct ti_bandgap_data omap4470_data;
#else
#define omap4430_data					NULL
#define omap4460_data					NULL
#define omap4470_data					NULL
#endif

#ifdef CONFIG_OMAP5_THERMAL
extern const struct ti_bandgap_data omap5430_data;
#else
#define omap5430_data					NULL
#endif

#ifdef CONFIG_DRA752_THERMAL
extern const struct ti_bandgap_data dra752_data;
#else
#define dra752_data					NULL
#endif
#endif
