
 

#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/slab.h>

 
#define SI544_REG_CONTROL	7
#define SI544_REG_OE_STATE	17
#define SI544_REG_HS_DIV	23
#define SI544_REG_LS_HS_DIV	24
#define SI544_REG_FBDIV0	26
#define SI544_REG_FBDIV8	27
#define SI544_REG_FBDIV16	28
#define SI544_REG_FBDIV24	29
#define SI544_REG_FBDIV32	30
#define SI544_REG_FBDIV40	31
#define SI544_REG_FCAL_OVR	69
#define SI544_REG_ADPLL_DELTA_M0	231
#define SI544_REG_ADPLL_DELTA_M8	232
#define SI544_REG_ADPLL_DELTA_M16	233
#define SI544_REG_PAGE_SELECT	255

 
#define SI544_CONTROL_RESET	BIT(7)
#define SI544_CONTROL_MS_ICAL2	BIT(3)

#define SI544_OE_STATE_ODC_OE	BIT(0)

 
#define SI544_MIN_FREQ	    200000U

 
#define FXO		  55050000U

 
#define FVCO_MIN       10800000000ULL

#define HS_DIV_MAX	2046
#define HS_DIV_MAX_ODD	33

 
#define MIN_HSDIV_FREQ	(FVCO_MIN / HS_DIV_MAX)

 
#define DELTA_M_MAX	8161512
#define DELTA_M_FRAC_NUM	19
#define DELTA_M_FRAC_DEN	20000

enum si544_speed_grade {
	si544a,
	si544b,
	si544c,
};

struct clk_si544 {
	struct clk_hw hw;
	struct regmap *regmap;
	struct i2c_client *i2c_client;
	enum si544_speed_grade speed_grade;
};
#define to_clk_si544(_hw)	container_of(_hw, struct clk_si544, hw)

 
struct clk_si544_muldiv {
	u32 fb_div_frac;
	u16 fb_div_int;
	u16 hs_div;
	u8 ls_div_bits;
	s32 delta_m;
};

 
static int si544_enable_output(struct clk_si544 *data, bool enable)
{
	return regmap_update_bits(data->regmap, SI544_REG_OE_STATE,
		SI544_OE_STATE_ODC_OE, enable ? SI544_OE_STATE_ODC_OE : 0);
}

static int si544_prepare(struct clk_hw *hw)
{
	struct clk_si544 *data = to_clk_si544(hw);

	return si544_enable_output(data, true);
}

static void si544_unprepare(struct clk_hw *hw)
{
	struct clk_si544 *data = to_clk_si544(hw);

	si544_enable_output(data, false);
}

static int si544_is_prepared(struct clk_hw *hw)
{
	struct clk_si544 *data = to_clk_si544(hw);
	unsigned int val;
	int err;

	err = regmap_read(data->regmap, SI544_REG_OE_STATE, &val);
	if (err < 0)
		return err;

	return !!(val & SI544_OE_STATE_ODC_OE);
}

 
static int si544_get_muldiv(struct clk_si544 *data,
	struct clk_si544_muldiv *settings)
{
	int err;
	u8 reg[6];

	err = regmap_bulk_read(data->regmap, SI544_REG_HS_DIV, reg, 2);
	if (err)
		return err;

	settings->ls_div_bits = (reg[1] >> 4) & 0x07;
	settings->hs_div = (reg[1] & 0x07) << 8 | reg[0];

	err = regmap_bulk_read(data->regmap, SI544_REG_FBDIV0, reg, 6);
	if (err)
		return err;

	settings->fb_div_int = reg[4] | (reg[5] & 0x07) << 8;
	settings->fb_div_frac = reg[0] | reg[1] << 8 | reg[2] << 16 |
				reg[3] << 24;

	err = regmap_bulk_read(data->regmap, SI544_REG_ADPLL_DELTA_M0, reg, 3);
	if (err)
		return err;

	 
	settings->delta_m = reg[0] << 8 | reg[1] << 16 | reg[2] << 24;
	settings->delta_m >>= 8;

	return 0;
}

static int si544_set_delta_m(struct clk_si544 *data, s32 delta_m)
{
	u8 reg[3];

	reg[0] = delta_m;
	reg[1] = delta_m >> 8;
	reg[2] = delta_m >> 16;

	return regmap_bulk_write(data->regmap, SI544_REG_ADPLL_DELTA_M0,
				 reg, 3);
}

static int si544_set_muldiv(struct clk_si544 *data,
	struct clk_si544_muldiv *settings)
{
	int err;
	u8 reg[6];

	reg[0] = settings->hs_div;
	reg[1] = settings->hs_div >> 8 | settings->ls_div_bits << 4;

	err = regmap_bulk_write(data->regmap, SI544_REG_HS_DIV, reg, 2);
	if (err < 0)
		return err;

	reg[0] = settings->fb_div_frac;
	reg[1] = settings->fb_div_frac >> 8;
	reg[2] = settings->fb_div_frac >> 16;
	reg[3] = settings->fb_div_frac >> 24;
	reg[4] = settings->fb_div_int;
	reg[5] = settings->fb_div_int >> 8;

	 
	return regmap_bulk_write(data->regmap, SI544_REG_FBDIV0, reg, 6);
}

static bool is_valid_frequency(const struct clk_si544 *data,
	unsigned long frequency)
{
	unsigned long max_freq = 0;

	if (frequency < SI544_MIN_FREQ)
		return false;

	switch (data->speed_grade) {
	case si544a:
		max_freq = 1500000000;
		break;
	case si544b:
		max_freq = 800000000;
		break;
	case si544c:
		max_freq = 350000000;
		break;
	}

	return frequency <= max_freq;
}

 
static int si544_calc_muldiv(struct clk_si544_muldiv *settings,
	unsigned long frequency)
{
	u64 vco;
	u32 ls_freq;
	u32 tmp;
	u8 res;

	 
	ls_freq = frequency;
	settings->ls_div_bits = 0;

	if (frequency >= MIN_HSDIV_FREQ) {
		settings->ls_div_bits = 0;
	} else {
		res = 1;
		tmp = 2 * HS_DIV_MAX;
		while (tmp <= (HS_DIV_MAX * 32)) {
			if (((u64)frequency * tmp) >= FVCO_MIN)
				break;
			++res;
			tmp <<= 1;
		}
		settings->ls_div_bits = res;
		ls_freq = frequency << res;
	}

	 
	vco = FVCO_MIN + ls_freq - 1;
	do_div(vco, ls_freq);
	settings->hs_div = vco;

	 
	if ((settings->hs_div & 1) &&
	    (settings->hs_div > HS_DIV_MAX_ODD || settings->ls_div_bits))
		++settings->hs_div;

	 
	vco = (u64)ls_freq * settings->hs_div;

	 
	tmp = do_div(vco, FXO);
	settings->fb_div_int = vco;

	 
	vco = (u64)tmp << 32;
	vco += FXO / 2;  
	do_div(vco, FXO);
	settings->fb_div_frac = vco;

	 
	settings->delta_m = 0;

	return 0;
}

 
static unsigned long si544_calc_center_rate(
		const struct clk_si544_muldiv *settings)
{
	u32 d = settings->hs_div * BIT(settings->ls_div_bits);
	u64 vco;

	 
	vco = (u64)settings->fb_div_frac * FXO;
	vco += (FXO / 2);
	vco >>= 32;

	 
	vco += (u64)settings->fb_div_int * FXO;

	 
	do_div(vco, d);

	return vco;
}

static unsigned long si544_calc_rate(const struct clk_si544_muldiv *settings)
{
	unsigned long rate = si544_calc_center_rate(settings);
	s64 delta = (s64)rate * (DELTA_M_FRAC_NUM * settings->delta_m);

	 
	if (settings->delta_m < 0)
		delta -= ((s64)DELTA_M_MAX * DELTA_M_FRAC_DEN) / 2;
	else
		delta += ((s64)DELTA_M_MAX * DELTA_M_FRAC_DEN) / 2;
	delta = div64_s64(delta, ((s64)DELTA_M_MAX * DELTA_M_FRAC_DEN));

	return rate + delta;
}

static unsigned long si544_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_si544 *data = to_clk_si544(hw);
	struct clk_si544_muldiv settings;
	int err;

	err = si544_get_muldiv(data, &settings);
	if (err)
		return 0;

	return si544_calc_rate(&settings);
}

static long si544_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct clk_si544 *data = to_clk_si544(hw);

	if (!is_valid_frequency(data, rate))
		return -EINVAL;

	 
	return rate;
}

 
static unsigned long si544_max_delta(unsigned long rate)
{
	u64 num = rate;

	num *= DELTA_M_FRAC_NUM;
	do_div(num, DELTA_M_FRAC_DEN);

	return num;
}

static s32 si544_calc_delta(s32 delta, s32 max_delta)
{
	s64 n = (s64)delta * DELTA_M_MAX;

	return div_s64(n, max_delta);
}

static int si544_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct clk_si544 *data = to_clk_si544(hw);
	struct clk_si544_muldiv settings;
	unsigned long center;
	long max_delta;
	long delta;
	unsigned int old_oe_state;
	int err;

	if (!is_valid_frequency(data, rate))
		return -EINVAL;

	 
	err = si544_get_muldiv(data, &settings);
	if (err)
		return err;

	center = si544_calc_center_rate(&settings);
	max_delta = si544_max_delta(center);
	delta = rate - center;

	if (abs(delta) <= max_delta)
		return si544_set_delta_m(data,
					 si544_calc_delta(delta, max_delta));

	 
	err = si544_calc_muldiv(&settings, rate);
	if (err)
		return err;

	err = regmap_read(data->regmap, SI544_REG_OE_STATE, &old_oe_state);
	if (err)
		return err;

	si544_enable_output(data, false);

	 
	err = regmap_write(data->regmap, SI544_REG_FCAL_OVR, 0);
	if (err < 0)
		return err;

	err = si544_set_delta_m(data, settings.delta_m);
	if (err < 0)
		return err;

	err = si544_set_muldiv(data, &settings);
	if (err < 0)
		return err;  

	 
	err = regmap_write(data->regmap, SI544_REG_CONTROL,
			   SI544_CONTROL_MS_ICAL2);
	if (err < 0)
		return err;

	 
	usleep_range(10000, 12000);

	if (old_oe_state & SI544_OE_STATE_ODC_OE)
		si544_enable_output(data, true);

	return err;
}

static const struct clk_ops si544_clk_ops = {
	.prepare = si544_prepare,
	.unprepare = si544_unprepare,
	.is_prepared = si544_is_prepared,
	.recalc_rate = si544_recalc_rate,
	.round_rate = si544_round_rate,
	.set_rate = si544_set_rate,
};

static bool si544_regmap_is_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case SI544_REG_CONTROL:
	case SI544_REG_FCAL_OVR:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config si544_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.max_register = SI544_REG_PAGE_SELECT,
	.volatile_reg = si544_regmap_is_volatile,
};

static const struct i2c_device_id si544_id[] = {
	{ "si544a", si544a },
	{ "si544b", si544b },
	{ "si544c", si544c },
	{ }
};
MODULE_DEVICE_TABLE(i2c, si544_id);

static int si544_probe(struct i2c_client *client)
{
	struct clk_si544 *data;
	struct clk_init_data init;
	const struct i2c_device_id *id = i2c_match_id(si544_id, client);
	int err;

	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	init.ops = &si544_clk_ops;
	init.flags = 0;
	init.num_parents = 0;
	data->hw.init = &init;
	data->i2c_client = client;
	data->speed_grade = id->driver_data;

	if (of_property_read_string(client->dev.of_node, "clock-output-names",
			&init.name))
		init.name = client->dev.of_node->name;

	data->regmap = devm_regmap_init_i2c(client, &si544_regmap_config);
	if (IS_ERR(data->regmap))
		return PTR_ERR(data->regmap);

	i2c_set_clientdata(client, data);

	 
	err = regmap_write(data->regmap, SI544_REG_PAGE_SELECT, 0);
	if (err < 0)
		return err;

	err = devm_clk_hw_register(&client->dev, &data->hw);
	if (err) {
		dev_err(&client->dev, "clock registration failed\n");
		return err;
	}
	err = devm_of_clk_add_hw_provider(&client->dev, of_clk_hw_simple_get,
					  &data->hw);
	if (err) {
		dev_err(&client->dev, "unable to add clk provider\n");
		return err;
	}

	return 0;
}

static const struct of_device_id clk_si544_of_match[] = {
	{ .compatible = "silabs,si544a" },
	{ .compatible = "silabs,si544b" },
	{ .compatible = "silabs,si544c" },
	{ },
};
MODULE_DEVICE_TABLE(of, clk_si544_of_match);

static struct i2c_driver si544_driver = {
	.driver = {
		.name = "si544",
		.of_match_table = clk_si544_of_match,
	},
	.probe		= si544_probe,
	.id_table	= si544_id,
};
module_i2c_driver(si544_driver);

MODULE_AUTHOR("Mike Looijmans <mike.looijmans@topic.nl>");
MODULE_DESCRIPTION("Si544 driver");
MODULE_LICENSE("GPL");
