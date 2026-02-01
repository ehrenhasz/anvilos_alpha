
 

#include <linux/clk.h>
#include <linux/cpu_pm.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/reboot.h>
#include <linux/spinlock.h>
#include <linux/sys_soc.h>
#include <linux/types.h>

#include "ti-bandgap.h"

static int ti_bandgap_force_single_read(struct ti_bandgap *bgp, int id);
#ifdef CONFIG_PM_SLEEP
static int bandgap_omap_cpu_notifier(struct notifier_block *nb,
				  unsigned long cmd, void *v);
#endif

 

 
static u32 ti_bandgap_readl(struct ti_bandgap *bgp, u32 reg)
{
	return readl(bgp->base + reg);
}

 
static void ti_bandgap_writel(struct ti_bandgap *bgp, u32 val, u32 reg)
{
	writel(val, bgp->base + reg);
}

 
#define RMW_BITS(bgp, id, reg, mask, val)			\
do {								\
	struct temp_sensor_registers *t;			\
	u32 r;							\
								\
	t = bgp->conf->sensors[(id)].registers;		\
	r = ti_bandgap_readl(bgp, t->reg);			\
	r &= ~t->mask;						\
	r |= (val) << __ffs(t->mask);				\
	ti_bandgap_writel(bgp, r, t->reg);			\
} while (0)

 

 
static int ti_bandgap_power(struct ti_bandgap *bgp, bool on)
{
	int i;

	if (!TI_BANDGAP_HAS(bgp, POWER_SWITCH))
		return -ENOTSUPP;

	for (i = 0; i < bgp->conf->sensor_count; i++)
		 
		RMW_BITS(bgp, i, temp_sensor_ctrl, bgap_tempsoff_mask, !on);
	return 0;
}

 
static u32 ti_errata814_bandgap_read_temp(struct ti_bandgap *bgp,  u32 reg)
{
	u32 val1, val2;

	val1 = ti_bandgap_readl(bgp, reg);
	val2 = ti_bandgap_readl(bgp, reg);

	 
	if (val1 == val2)
		return val1;

	 
	return ti_bandgap_readl(bgp, reg);
}

 
static u32 ti_bandgap_read_temp(struct ti_bandgap *bgp, int id)
{
	struct temp_sensor_registers *tsr;
	u32 temp, reg;

	tsr = bgp->conf->sensors[id].registers;
	reg = tsr->temp_sensor_ctrl;

	if (TI_BANDGAP_HAS(bgp, FREEZE_BIT)) {
		RMW_BITS(bgp, id, bgap_mask_ctrl, mask_freeze_mask, 1);
		 
		reg = tsr->ctrl_dtemp_1;
	}

	 
	if (TI_BANDGAP_HAS(bgp, ERRATA_814))
		temp = ti_errata814_bandgap_read_temp(bgp, reg);
	else
		temp = ti_bandgap_readl(bgp, reg);

	temp &= tsr->bgap_dtemp_mask;

	if (TI_BANDGAP_HAS(bgp, FREEZE_BIT))
		RMW_BITS(bgp, id, bgap_mask_ctrl, mask_freeze_mask, 0);

	return temp;
}

 

 
static irqreturn_t ti_bandgap_talert_irq_handler(int irq, void *data)
{
	struct ti_bandgap *bgp = data;
	struct temp_sensor_registers *tsr;
	u32 t_hot = 0, t_cold = 0, ctrl;
	int i;

	spin_lock(&bgp->lock);
	for (i = 0; i < bgp->conf->sensor_count; i++) {
		tsr = bgp->conf->sensors[i].registers;
		ctrl = ti_bandgap_readl(bgp, tsr->bgap_status);

		 
		t_hot = ctrl & tsr->status_hot_mask;

		 
		t_cold = ctrl & tsr->status_cold_mask;

		if (!t_cold && !t_hot)
			continue;

		ctrl = ti_bandgap_readl(bgp, tsr->bgap_mask_ctrl);
		 
		if (t_hot) {
			ctrl &= ~tsr->mask_hot_mask;
			ctrl |= tsr->mask_cold_mask;
		} else if (t_cold) {
			ctrl &= ~tsr->mask_cold_mask;
			ctrl |= tsr->mask_hot_mask;
		}

		ti_bandgap_writel(bgp, ctrl, tsr->bgap_mask_ctrl);

		dev_dbg(bgp->dev,
			"%s: IRQ from %s sensor: hotevent %d coldevent %d\n",
			__func__, bgp->conf->sensors[i].domain,
			t_hot, t_cold);

		 
		if (bgp->conf->report_temperature)
			bgp->conf->report_temperature(bgp, i);
	}
	spin_unlock(&bgp->lock);

	return IRQ_HANDLED;
}

 
static irqreturn_t ti_bandgap_tshut_irq_handler(int irq, void *data)
{
	pr_emerg("%s: TSHUT temperature reached. Needs shut down...\n",
		 __func__);

	orderly_poweroff(true);

	return IRQ_HANDLED;
}

 

 
static
int ti_bandgap_adc_to_mcelsius(struct ti_bandgap *bgp, int adc_val, int *t)
{
	const struct ti_bandgap_data *conf = bgp->conf;

	 
	if (adc_val < conf->adc_start_val || adc_val > conf->adc_end_val)
		return -ERANGE;

	*t = bgp->conf->conv_table[adc_val - conf->adc_start_val];
	return 0;
}

 
static inline int ti_bandgap_validate(struct ti_bandgap *bgp, int id)
{
	if (IS_ERR_OR_NULL(bgp)) {
		pr_err("%s: invalid bandgap pointer\n", __func__);
		return -EINVAL;
	}

	if ((id < 0) || (id >= bgp->conf->sensor_count)) {
		dev_err(bgp->dev, "%s: sensor id out of range (%d)\n",
			__func__, id);
		return -ERANGE;
	}

	return 0;
}

 
static void ti_bandgap_read_counter(struct ti_bandgap *bgp, int id,
				    int *interval)
{
	struct temp_sensor_registers *tsr;
	int time;

	tsr = bgp->conf->sensors[id].registers;
	time = ti_bandgap_readl(bgp, tsr->bgap_counter);
	time = (time & tsr->counter_mask) >>
					__ffs(tsr->counter_mask);
	time = time * 1000 / bgp->clk_rate;
	*interval = time;
}

 
static void ti_bandgap_read_counter_delay(struct ti_bandgap *bgp, int id,
					  int *interval)
{
	struct temp_sensor_registers *tsr;
	int reg_val;

	tsr = bgp->conf->sensors[id].registers;

	reg_val = ti_bandgap_readl(bgp, tsr->bgap_mask_ctrl);
	reg_val = (reg_val & tsr->mask_counter_delay_mask) >>
				__ffs(tsr->mask_counter_delay_mask);
	switch (reg_val) {
	case 0:
		*interval = 0;
		break;
	case 1:
		*interval = 1;
		break;
	case 2:
		*interval = 10;
		break;
	case 3:
		*interval = 100;
		break;
	case 4:
		*interval = 250;
		break;
	case 5:
		*interval = 500;
		break;
	default:
		dev_warn(bgp->dev, "Wrong counter delay value read from register %X",
			 reg_val);
	}
}

 
int ti_bandgap_read_update_interval(struct ti_bandgap *bgp, int id,
				    int *interval)
{
	int ret = 0;

	ret = ti_bandgap_validate(bgp, id);
	if (ret)
		goto exit;

	if (!TI_BANDGAP_HAS(bgp, COUNTER) &&
	    !TI_BANDGAP_HAS(bgp, COUNTER_DELAY)) {
		ret = -ENOTSUPP;
		goto exit;
	}

	if (TI_BANDGAP_HAS(bgp, COUNTER)) {
		ti_bandgap_read_counter(bgp, id, interval);
		goto exit;
	}

	ti_bandgap_read_counter_delay(bgp, id, interval);
exit:
	return ret;
}

 
static int ti_bandgap_write_counter_delay(struct ti_bandgap *bgp, int id,
					  u32 interval)
{
	int rval;

	switch (interval) {
	case 0:  
		rval = 0x0;
		break;
	case 1:  
		rval = 0x1;
		break;
	case 10:  
		rval = 0x2;
		break;
	case 100:  
		rval = 0x3;
		break;
	case 250:  
		rval = 0x4;
		break;
	case 500:  
		rval = 0x5;
		break;
	default:
		dev_warn(bgp->dev, "Delay %d ms is not supported\n", interval);
		return -EINVAL;
	}

	spin_lock(&bgp->lock);
	RMW_BITS(bgp, id, bgap_mask_ctrl, mask_counter_delay_mask, rval);
	spin_unlock(&bgp->lock);

	return 0;
}

 
static void ti_bandgap_write_counter(struct ti_bandgap *bgp, int id,
				     u32 interval)
{
	interval = interval * bgp->clk_rate / 1000;
	spin_lock(&bgp->lock);
	RMW_BITS(bgp, id, bgap_counter, counter_mask, interval);
	spin_unlock(&bgp->lock);
}

 
int ti_bandgap_write_update_interval(struct ti_bandgap *bgp,
				     int id, u32 interval)
{
	int ret = ti_bandgap_validate(bgp, id);
	if (ret)
		goto exit;

	if (!TI_BANDGAP_HAS(bgp, COUNTER) &&
	    !TI_BANDGAP_HAS(bgp, COUNTER_DELAY)) {
		ret = -ENOTSUPP;
		goto exit;
	}

	if (TI_BANDGAP_HAS(bgp, COUNTER)) {
		ti_bandgap_write_counter(bgp, id, interval);
		goto exit;
	}

	ret = ti_bandgap_write_counter_delay(bgp, id, interval);
exit:
	return ret;
}

 
int ti_bandgap_read_temperature(struct ti_bandgap *bgp, int id,
				int *temperature)
{
	u32 temp;
	int ret;

	ret = ti_bandgap_validate(bgp, id);
	if (ret)
		return ret;

	if (!TI_BANDGAP_HAS(bgp, MODE_CONFIG)) {
		ret = ti_bandgap_force_single_read(bgp, id);
		if (ret)
			return ret;
	}

	spin_lock(&bgp->lock);
	temp = ti_bandgap_read_temp(bgp, id);
	spin_unlock(&bgp->lock);

	ret = ti_bandgap_adc_to_mcelsius(bgp, temp, &temp);
	if (ret)
		return -EIO;

	*temperature = temp;

	return 0;
}

 
int ti_bandgap_set_sensor_data(struct ti_bandgap *bgp, int id, void *data)
{
	int ret = ti_bandgap_validate(bgp, id);
	if (ret)
		return ret;

	bgp->regval[id].data = data;

	return 0;
}

 
void *ti_bandgap_get_sensor_data(struct ti_bandgap *bgp, int id)
{
	int ret = ti_bandgap_validate(bgp, id);
	if (ret)
		return ERR_PTR(ret);

	return bgp->regval[id].data;
}

 

 
static int
ti_bandgap_force_single_read(struct ti_bandgap *bgp, int id)
{
	struct temp_sensor_registers *tsr = bgp->conf->sensors[id].registers;
	void __iomem *temp_sensor_ctrl = bgp->base + tsr->temp_sensor_ctrl;
	int error;
	u32 val;

	 
	if (TI_BANDGAP_HAS(bgp, MODE_CONFIG)) {
		if (TI_BANDGAP_HAS(bgp, CONT_MODE_ONLY))
			RMW_BITS(bgp, id, bgap_mode_ctrl, mode_ctrl_mask, 1);
		else
			RMW_BITS(bgp, id, bgap_mode_ctrl, mode_ctrl_mask, 0);
	}

	 
	if (tsr->bgap_soc_mask) {
		RMW_BITS(bgp, id, temp_sensor_ctrl, bgap_soc_mask, 1);

		 
		error = readl_poll_timeout_atomic(temp_sensor_ctrl, val,
						  val & tsr->bgap_eocz_mask,
						  1, 1000);
		if (error)
			dev_warn(bgp->dev, "eocz timed out waiting high\n");

		 
		RMW_BITS(bgp, id, temp_sensor_ctrl, bgap_soc_mask, 0);
	}

	 
	error = readl_poll_timeout_atomic(temp_sensor_ctrl, val,
					  !(val & tsr->bgap_eocz_mask),
					  1, 1500);
	if (error)
		dev_warn(bgp->dev, "eocz timed out waiting low\n");

	return 0;
}

 
static int ti_bandgap_set_continuous_mode(struct ti_bandgap *bgp)
{
	int i;

	for (i = 0; i < bgp->conf->sensor_count; i++) {
		 
		ti_bandgap_force_single_read(bgp, i);
		RMW_BITS(bgp, i, bgap_mode_ctrl, mode_ctrl_mask, 1);
	}

	return 0;
}

 
int ti_bandgap_get_trend(struct ti_bandgap *bgp, int id, int *trend)
{
	struct temp_sensor_registers *tsr;
	u32 temp1, temp2, reg1, reg2;
	int t1, t2, interval, ret = 0;

	ret = ti_bandgap_validate(bgp, id);
	if (ret)
		goto exit;

	if (!TI_BANDGAP_HAS(bgp, HISTORY_BUFFER) ||
	    !TI_BANDGAP_HAS(bgp, FREEZE_BIT)) {
		ret = -ENOTSUPP;
		goto exit;
	}

	spin_lock(&bgp->lock);

	tsr = bgp->conf->sensors[id].registers;

	 
	RMW_BITS(bgp, id, bgap_mask_ctrl, mask_freeze_mask, 1);
	reg1 = tsr->ctrl_dtemp_1;
	reg2 = tsr->ctrl_dtemp_2;

	 
	temp1 = ti_bandgap_readl(bgp, reg1);
	temp1 &= tsr->bgap_dtemp_mask;

	temp2 = ti_bandgap_readl(bgp, reg2);
	temp2 &= tsr->bgap_dtemp_mask;

	 
	ret = ti_bandgap_adc_to_mcelsius(bgp, temp1, &t1);
	if (ret)
		goto unfreeze;

	ret = ti_bandgap_adc_to_mcelsius(bgp, temp2, &t2);
	if (ret)
		goto unfreeze;

	 
	ret = ti_bandgap_read_update_interval(bgp, id, &interval);
	if (ret)
		goto unfreeze;

	 
	if (interval == 0)
		interval = 1;

	*trend = (t1 - t2) / interval;

	dev_dbg(bgp->dev, "The temperatures are t1 = %d and t2 = %d and trend =%d\n",
		t1, t2, *trend);

unfreeze:
	RMW_BITS(bgp, id, bgap_mask_ctrl, mask_freeze_mask, 0);
	spin_unlock(&bgp->lock);
exit:
	return ret;
}

 
static int ti_bandgap_tshut_init(struct ti_bandgap *bgp,
				 struct platform_device *pdev)
{
	int status;

	status = request_irq(gpiod_to_irq(bgp->tshut_gpiod),
			     ti_bandgap_tshut_irq_handler,
			     IRQF_TRIGGER_RISING, "tshut", NULL);
	if (status)
		dev_err(bgp->dev, "request irq failed for TSHUT");

	return 0;
}

 
static int ti_bandgap_talert_init(struct ti_bandgap *bgp,
				  struct platform_device *pdev)
{
	int ret;

	bgp->irq = platform_get_irq(pdev, 0);
	if (bgp->irq < 0)
		return bgp->irq;

	ret = request_threaded_irq(bgp->irq, NULL,
				   ti_bandgap_talert_irq_handler,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "talert", bgp);
	if (ret) {
		dev_err(&pdev->dev, "Request threaded irq failed.\n");
		return ret;
	}

	return 0;
}

static const struct of_device_id of_ti_bandgap_match[];
 
static struct ti_bandgap *ti_bandgap_build(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *of_id;
	struct ti_bandgap *bgp;
	struct resource *res;
	int i;

	 
	if (!node) {
		dev_err(&pdev->dev, "no platform information available\n");
		return ERR_PTR(-EINVAL);
	}

	bgp = devm_kzalloc(&pdev->dev, sizeof(*bgp), GFP_KERNEL);
	if (!bgp)
		return ERR_PTR(-ENOMEM);

	of_id = of_match_device(of_ti_bandgap_match, &pdev->dev);
	if (of_id)
		bgp->conf = of_id->data;

	 
	bgp->regval = devm_kcalloc(&pdev->dev, bgp->conf->sensor_count,
				   sizeof(*bgp->regval), GFP_KERNEL);
	if (!bgp->regval)
		return ERR_PTR(-ENOMEM);

	i = 0;
	do {
		void __iomem *chunk;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res)
			break;
		chunk = devm_ioremap_resource(&pdev->dev, res);
		if (i == 0)
			bgp->base = chunk;
		if (IS_ERR(chunk))
			return ERR_CAST(chunk);

		i++;
	} while (res);

	if (TI_BANDGAP_HAS(bgp, TSHUT)) {
		bgp->tshut_gpiod = devm_gpiod_get(&pdev->dev, NULL, GPIOD_IN);
		if (IS_ERR(bgp->tshut_gpiod)) {
			dev_err(&pdev->dev, "invalid gpio for tshut\n");
			return ERR_CAST(bgp->tshut_gpiod);
		}
	}

	return bgp;
}

 
static const struct soc_device_attribute soc_no_cpu_notifier[] = {
	{ .machine = "OMAP4430" },
	{   }
};

 

static
int ti_bandgap_probe(struct platform_device *pdev)
{
	struct ti_bandgap *bgp;
	int clk_rate, ret, i;

	bgp = ti_bandgap_build(pdev);
	if (IS_ERR(bgp)) {
		dev_err(&pdev->dev, "failed to fetch platform data\n");
		return PTR_ERR(bgp);
	}
	bgp->dev = &pdev->dev;

	if (TI_BANDGAP_HAS(bgp, UNRELIABLE))
		dev_warn(&pdev->dev,
			 "This OMAP thermal sensor is unreliable. You've been warned\n");

	if (TI_BANDGAP_HAS(bgp, TSHUT)) {
		ret = ti_bandgap_tshut_init(bgp, pdev);
		if (ret) {
			dev_err(&pdev->dev,
				"failed to initialize system tshut IRQ\n");
			return ret;
		}
	}

	bgp->fclock = clk_get(NULL, bgp->conf->fclock_name);
	if (IS_ERR(bgp->fclock)) {
		dev_err(&pdev->dev, "failed to request fclock reference\n");
		ret = PTR_ERR(bgp->fclock);
		goto free_irqs;
	}

	bgp->div_clk = clk_get(NULL, bgp->conf->div_ck_name);
	if (IS_ERR(bgp->div_clk)) {
		dev_err(&pdev->dev, "failed to request div_ts_ck clock ref\n");
		ret = PTR_ERR(bgp->div_clk);
		goto put_fclock;
	}

	for (i = 0; i < bgp->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		u32 val;

		tsr = bgp->conf->sensors[i].registers;
		 
		val = ti_bandgap_readl(bgp, tsr->bgap_efuse);
		if (!val)
			dev_info(&pdev->dev,
				 "Non-trimmed BGAP, Temp not accurate\n");
	}

	clk_rate = clk_round_rate(bgp->div_clk,
				  bgp->conf->sensors[0].ts_data->max_freq);
	if (clk_rate < bgp->conf->sensors[0].ts_data->min_freq ||
	    clk_rate <= 0) {
		ret = -ENODEV;
		dev_err(&pdev->dev, "wrong clock rate (%d)\n", clk_rate);
		goto put_clks;
	}

	ret = clk_set_rate(bgp->div_clk, clk_rate);
	if (ret)
		dev_err(&pdev->dev, "Cannot re-set clock rate. Continuing\n");

	bgp->clk_rate = clk_rate;
	if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
		clk_prepare_enable(bgp->fclock);


	spin_lock_init(&bgp->lock);
	bgp->dev = &pdev->dev;
	platform_set_drvdata(pdev, bgp);

	ti_bandgap_power(bgp, true);

	 
	if (TI_BANDGAP_HAS(bgp, COUNTER))
		for (i = 0; i < bgp->conf->sensor_count; i++)
			RMW_BITS(bgp, i, bgap_counter, counter_mask, 1);

	 
	for (i = 0; i < bgp->conf->sensor_count; i++) {
		struct temp_sensor_data *ts_data;

		ts_data = bgp->conf->sensors[i].ts_data;

		if (TI_BANDGAP_HAS(bgp, TALERT)) {
			 
			RMW_BITS(bgp, i, bgap_threshold,
				 threshold_tcold_mask, ts_data->t_cold);
			RMW_BITS(bgp, i, bgap_threshold,
				 threshold_thot_mask, ts_data->t_hot);
			 
			RMW_BITS(bgp, i, bgap_mask_ctrl, mask_hot_mask, 1);
			RMW_BITS(bgp, i, bgap_mask_ctrl, mask_cold_mask, 1);
		}

		if (TI_BANDGAP_HAS(bgp, TSHUT_CONFIG)) {
			 
			RMW_BITS(bgp, i, tshut_threshold,
				 tshut_hot_mask, ts_data->tshut_hot);
			RMW_BITS(bgp, i, tshut_threshold,
				 tshut_cold_mask, ts_data->tshut_cold);
		}
	}

	if (TI_BANDGAP_HAS(bgp, MODE_CONFIG))
		ti_bandgap_set_continuous_mode(bgp);

	 
	if (TI_BANDGAP_HAS(bgp, COUNTER))
		for (i = 0; i < bgp->conf->sensor_count; i++)
			RMW_BITS(bgp, i, bgap_counter, counter_mask,
				 bgp->clk_rate / 4);

	 
	for (i = 0; i < bgp->conf->sensor_count; i++) {
		char *domain;

		if (bgp->conf->sensors[i].register_cooling) {
			ret = bgp->conf->sensors[i].register_cooling(bgp, i);
			if (ret)
				goto remove_sensors;
		}

		if (bgp->conf->expose_sensor) {
			domain = bgp->conf->sensors[i].domain;
			ret = bgp->conf->expose_sensor(bgp, i, domain);
			if (ret)
				goto remove_last_cooling;
		}
	}

	 
	if (TI_BANDGAP_HAS(bgp, TALERT)) {
		ret = ti_bandgap_talert_init(bgp, pdev);
		if (ret) {
			dev_err(&pdev->dev, "failed to initialize Talert IRQ\n");
			i = bgp->conf->sensor_count;
			goto disable_clk;
		}
	}

#ifdef CONFIG_PM_SLEEP
	bgp->nb.notifier_call = bandgap_omap_cpu_notifier;
	if (!soc_device_match(soc_no_cpu_notifier))
		cpu_pm_register_notifier(&bgp->nb);
#endif

	return 0;

remove_last_cooling:
	if (bgp->conf->sensors[i].unregister_cooling)
		bgp->conf->sensors[i].unregister_cooling(bgp, i);
remove_sensors:
	for (i--; i >= 0; i--) {
		if (bgp->conf->sensors[i].unregister_cooling)
			bgp->conf->sensors[i].unregister_cooling(bgp, i);
		if (bgp->conf->remove_sensor)
			bgp->conf->remove_sensor(bgp, i);
	}
	ti_bandgap_power(bgp, false);
disable_clk:
	if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
		clk_disable_unprepare(bgp->fclock);
put_clks:
	clk_put(bgp->div_clk);
put_fclock:
	clk_put(bgp->fclock);
free_irqs:
	if (TI_BANDGAP_HAS(bgp, TSHUT))
		free_irq(gpiod_to_irq(bgp->tshut_gpiod), NULL);

	return ret;
}

static
int ti_bandgap_remove(struct platform_device *pdev)
{
	struct ti_bandgap *bgp = platform_get_drvdata(pdev);
	int i;

	if (!soc_device_match(soc_no_cpu_notifier))
		cpu_pm_unregister_notifier(&bgp->nb);

	 
	for (i = 0; i < bgp->conf->sensor_count; i++) {
		if (bgp->conf->sensors[i].unregister_cooling)
			bgp->conf->sensors[i].unregister_cooling(bgp, i);

		if (bgp->conf->remove_sensor)
			bgp->conf->remove_sensor(bgp, i);
	}

	ti_bandgap_power(bgp, false);

	if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
		clk_disable_unprepare(bgp->fclock);
	clk_put(bgp->fclock);
	clk_put(bgp->div_clk);

	if (TI_BANDGAP_HAS(bgp, TALERT))
		free_irq(bgp->irq, bgp);

	if (TI_BANDGAP_HAS(bgp, TSHUT))
		free_irq(gpiod_to_irq(bgp->tshut_gpiod), NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ti_bandgap_save_ctxt(struct ti_bandgap *bgp)
{
	int i;

	for (i = 0; i < bgp->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;

		rval = &bgp->regval[i];
		tsr = bgp->conf->sensors[i].registers;

		if (TI_BANDGAP_HAS(bgp, MODE_CONFIG))
			rval->bg_mode_ctrl = ti_bandgap_readl(bgp,
							tsr->bgap_mode_ctrl);
		if (TI_BANDGAP_HAS(bgp, COUNTER))
			rval->bg_counter = ti_bandgap_readl(bgp,
							tsr->bgap_counter);
		if (TI_BANDGAP_HAS(bgp, TALERT)) {
			rval->bg_threshold = ti_bandgap_readl(bgp,
							tsr->bgap_threshold);
			rval->bg_ctrl = ti_bandgap_readl(bgp,
						   tsr->bgap_mask_ctrl);
		}

		if (TI_BANDGAP_HAS(bgp, TSHUT_CONFIG))
			rval->tshut_threshold = ti_bandgap_readl(bgp,
						   tsr->tshut_threshold);
	}

	return 0;
}

static int ti_bandgap_restore_ctxt(struct ti_bandgap *bgp)
{
	int i;

	for (i = 0; i < bgp->conf->sensor_count; i++) {
		struct temp_sensor_registers *tsr;
		struct temp_sensor_regval *rval;

		rval = &bgp->regval[i];
		tsr = bgp->conf->sensors[i].registers;

		if (TI_BANDGAP_HAS(bgp, TSHUT_CONFIG))
			ti_bandgap_writel(bgp, rval->tshut_threshold,
					  tsr->tshut_threshold);
		 
		ti_bandgap_force_single_read(bgp, i);

		if (TI_BANDGAP_HAS(bgp, COUNTER))
			ti_bandgap_writel(bgp, rval->bg_counter,
					  tsr->bgap_counter);
		if (TI_BANDGAP_HAS(bgp, MODE_CONFIG))
			ti_bandgap_writel(bgp, rval->bg_mode_ctrl,
					  tsr->bgap_mode_ctrl);
		if (TI_BANDGAP_HAS(bgp, TALERT)) {
			ti_bandgap_writel(bgp, rval->bg_threshold,
					  tsr->bgap_threshold);
			ti_bandgap_writel(bgp, rval->bg_ctrl,
					  tsr->bgap_mask_ctrl);
		}
	}

	return 0;
}

static int ti_bandgap_suspend(struct device *dev)
{
	struct ti_bandgap *bgp = dev_get_drvdata(dev);
	int err;

	err = ti_bandgap_save_ctxt(bgp);
	ti_bandgap_power(bgp, false);

	if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
		clk_disable_unprepare(bgp->fclock);

	bgp->is_suspended = true;

	return err;
}

static int bandgap_omap_cpu_notifier(struct notifier_block *nb,
				  unsigned long cmd, void *v)
{
	struct ti_bandgap *bgp;

	bgp = container_of(nb, struct ti_bandgap, nb);

	spin_lock(&bgp->lock);
	switch (cmd) {
	case CPU_CLUSTER_PM_ENTER:
		if (bgp->is_suspended)
			break;
		ti_bandgap_save_ctxt(bgp);
		ti_bandgap_power(bgp, false);
		if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
			clk_disable(bgp->fclock);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		if (bgp->is_suspended)
			break;
		if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
			clk_enable(bgp->fclock);
		ti_bandgap_power(bgp, true);
		ti_bandgap_restore_ctxt(bgp);
		break;
	}
	spin_unlock(&bgp->lock);

	return NOTIFY_OK;
}

static int ti_bandgap_resume(struct device *dev)
{
	struct ti_bandgap *bgp = dev_get_drvdata(dev);

	if (TI_BANDGAP_HAS(bgp, CLK_CTRL))
		clk_prepare_enable(bgp->fclock);

	ti_bandgap_power(bgp, true);
	bgp->is_suspended = false;

	return ti_bandgap_restore_ctxt(bgp);
}
static SIMPLE_DEV_PM_OPS(ti_bandgap_dev_pm_ops, ti_bandgap_suspend,
			 ti_bandgap_resume);

#define DEV_PM_OPS	(&ti_bandgap_dev_pm_ops)
#else
#define DEV_PM_OPS	NULL
#endif

static const struct of_device_id of_ti_bandgap_match[] = {
#ifdef CONFIG_OMAP3_THERMAL
	{
		.compatible = "ti,omap34xx-bandgap",
		.data = (void *)&omap34xx_data,
	},
	{
		.compatible = "ti,omap36xx-bandgap",
		.data = (void *)&omap36xx_data,
	},
#endif
#ifdef CONFIG_OMAP4_THERMAL
	{
		.compatible = "ti,omap4430-bandgap",
		.data = (void *)&omap4430_data,
	},
	{
		.compatible = "ti,omap4460-bandgap",
		.data = (void *)&omap4460_data,
	},
	{
		.compatible = "ti,omap4470-bandgap",
		.data = (void *)&omap4470_data,
	},
#endif
#ifdef CONFIG_OMAP5_THERMAL
	{
		.compatible = "ti,omap5430-bandgap",
		.data = (void *)&omap5430_data,
	},
#endif
#ifdef CONFIG_DRA752_THERMAL
	{
		.compatible = "ti,dra752-bandgap",
		.data = (void *)&dra752_data,
	},
#endif
	 
	{ },
};
MODULE_DEVICE_TABLE(of, of_ti_bandgap_match);

static struct platform_driver ti_bandgap_sensor_driver = {
	.probe = ti_bandgap_probe,
	.remove = ti_bandgap_remove,
	.driver = {
			.name = "ti-soc-thermal",
			.pm = DEV_PM_OPS,
			.of_match_table	= of_ti_bandgap_match,
	},
};

module_platform_driver(ti_bandgap_sensor_driver);

MODULE_DESCRIPTION("OMAP4+ bandgap temperature sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ti-soc-thermal");
MODULE_AUTHOR("Texas Instrument Inc.");
