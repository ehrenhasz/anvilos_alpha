
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-subdev.h>

 
#define IMX274_DEFAULT_FRAME_LENGTH		(4550)
#define IMX274_MAX_FRAME_LENGTH			(0x000fffff)

 
#define IMX274_PIXCLK_CONST1			(72000000)
#define IMX274_PIXCLK_CONST2			(1000000)

 
#define IMX274_GAIN_SHIFT			(8)
#define IMX274_GAIN_SHIFT_MASK			((1 << IMX274_GAIN_SHIFT) - 1)

 
#define IMX274_GAIN_REG_MAX			(1957)
#define IMX274_MIN_GAIN				(0x01 << IMX274_GAIN_SHIFT)
#define IMX274_MAX_ANALOG_GAIN			((2048 << IMX274_GAIN_SHIFT)\
					/ (2048 - IMX274_GAIN_REG_MAX))
#define IMX274_MAX_DIGITAL_GAIN			(8)
#define IMX274_DEF_GAIN				(20 << IMX274_GAIN_SHIFT)
#define IMX274_GAIN_CONST			(2048)  

 
#define IMX274_MIN_EXPOSURE_TIME		(4 * 260 / 72)

#define IMX274_MAX_WIDTH			(3840)
#define IMX274_MAX_HEIGHT			(2160)
#define IMX274_MAX_FRAME_RATE			(120)
#define IMX274_MIN_FRAME_RATE			(5)
#define IMX274_DEF_FRAME_RATE			(60)

 
#define IMX274_SHR_LIMIT_CONST			(4)

 
#define IMX274_RESET_DELAY1			(2000)
#define IMX274_RESET_DELAY2			(2200)

 
#define IMX274_SHIFT_8_BITS			(8)
#define IMX274_SHIFT_16_BITS			(16)
#define IMX274_MASK_LSB_2_BITS			(0x03)
#define IMX274_MASK_LSB_3_BITS			(0x07)
#define IMX274_MASK_LSB_4_BITS			(0x0f)
#define IMX274_MASK_LSB_8_BITS			(0x00ff)

#define DRIVER_NAME "IMX274"

 
#define IMX274_SHR_REG_MSB			0x300D  
#define IMX274_SHR_REG_LSB			0x300C  
#define IMX274_SVR_REG_MSB			0x300F  
#define IMX274_SVR_REG_LSB			0x300E  
#define IMX274_HTRIM_EN_REG			0x3037
#define IMX274_HTRIM_START_REG_LSB		0x3038
#define IMX274_HTRIM_START_REG_MSB		0x3039
#define IMX274_HTRIM_END_REG_LSB		0x303A
#define IMX274_HTRIM_END_REG_MSB		0x303B
#define IMX274_VWIDCUTEN_REG			0x30DD
#define IMX274_VWIDCUT_REG_LSB			0x30DE
#define IMX274_VWIDCUT_REG_MSB			0x30DF
#define IMX274_VWINPOS_REG_LSB			0x30E0
#define IMX274_VWINPOS_REG_MSB			0x30E1
#define IMX274_WRITE_VSIZE_REG_LSB		0x3130
#define IMX274_WRITE_VSIZE_REG_MSB		0x3131
#define IMX274_Y_OUT_SIZE_REG_LSB		0x3132
#define IMX274_Y_OUT_SIZE_REG_MSB		0x3133
#define IMX274_VMAX_REG_1			0x30FA  
#define IMX274_VMAX_REG_2			0x30F9  
#define IMX274_VMAX_REG_3			0x30F8  
#define IMX274_HMAX_REG_MSB			0x30F7  
#define IMX274_HMAX_REG_LSB			0x30F6  
#define IMX274_ANALOG_GAIN_ADDR_LSB		0x300A  
#define IMX274_ANALOG_GAIN_ADDR_MSB		0x300B  
#define IMX274_DIGITAL_GAIN_REG			0x3012  
#define IMX274_VFLIP_REG			0x301A  
#define IMX274_TEST_PATTERN_REG			0x303D  
#define IMX274_STANDBY_REG			0x3000  

#define IMX274_TABLE_WAIT_MS			0
#define IMX274_TABLE_END			1

 
static const char * const imx274_supply_names[] = {
	"vddl",   
	"vdig",   
	"vana",   
};

#define IMX274_NUM_SUPPLIES ARRAY_SIZE(imx274_supply_names)

 
struct reg_8 {
	u16 addr;
	u8 val;
};

static const struct regmap_config imx274_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

 
struct imx274_mode {
	const struct reg_8 *init_regs;
	u8 wbin_ratio;
	u8 hbin_ratio;
	int min_frame_len;
	int min_SHR;
	int max_fps;
	int nocpiop;
};

 
enum {
	TEST_PATTERN_DISABLED = 0,
	TEST_PATTERN_ALL_000H,
	TEST_PATTERN_ALL_FFFH,
	TEST_PATTERN_ALL_555H,
	TEST_PATTERN_ALL_AAAH,
	TEST_PATTERN_VSP_5AH,  
	TEST_PATTERN_VSP_A5H,  
	TEST_PATTERN_VSP_05H,  
	TEST_PATTERN_VSP_50H,  
	TEST_PATTERN_VSP_0FH,  
	TEST_PATTERN_VSP_F0H,  
	TEST_PATTERN_H_COLOR_BARS,
	TEST_PATTERN_V_COLOR_BARS,
};

static const char * const tp_qmenu[] = {
	"Disabled",
	"All 000h Pattern",
	"All FFFh Pattern",
	"All 555h Pattern",
	"All AAAh Pattern",
	"Vertical Stripe (555h / AAAh)",
	"Vertical Stripe (AAAh / 555h)",
	"Vertical Stripe (000h / 555h)",
	"Vertical Stripe (555h / 000h)",
	"Vertical Stripe (000h / FFFh)",
	"Vertical Stripe (FFFh / 000h)",
	"Vertical Color Bars",
	"Horizontal Color Bars",
};

 
static const struct reg_8 imx274_mode1_3840x2160_raw10[] = {
	{0x3004, 0x01},
	{0x3005, 0x01},
	{0x3006, 0x00},
	{0x3007, 0xa2},

	{0x3018, 0xA2},  

	{0x306B, 0x05},
	{0x30E2, 0x01},

	{0x30EE, 0x01},
	{0x3342, 0x0A},
	{0x3343, 0x00},
	{0x3344, 0x16},
	{0x3345, 0x00},
	{0x33A6, 0x01},
	{0x3528, 0x0E},
	{0x3554, 0x1F},
	{0x3555, 0x01},
	{0x3556, 0x01},
	{0x3557, 0x01},
	{0x3558, 0x01},
	{0x3559, 0x00},
	{0x355A, 0x00},
	{0x35BA, 0x0E},
	{0x366A, 0x1B},
	{0x366B, 0x1A},
	{0x366C, 0x19},
	{0x366D, 0x17},
	{0x3A41, 0x08},

	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_mode3_1920x1080_raw10[] = {
	{0x3004, 0x02},
	{0x3005, 0x21},
	{0x3006, 0x00},
	{0x3007, 0xb1},

	{0x3018, 0xA2},  

	{0x306B, 0x05},
	{0x30E2, 0x02},

	{0x30EE, 0x01},
	{0x3342, 0x0A},
	{0x3343, 0x00},
	{0x3344, 0x1A},
	{0x3345, 0x00},
	{0x33A6, 0x01},
	{0x3528, 0x0E},
	{0x3554, 0x00},
	{0x3555, 0x01},
	{0x3556, 0x01},
	{0x3557, 0x01},
	{0x3558, 0x01},
	{0x3559, 0x00},
	{0x355A, 0x00},
	{0x35BA, 0x0E},
	{0x366A, 0x1B},
	{0x366B, 0x1A},
	{0x366C, 0x19},
	{0x366D, 0x17},
	{0x3A41, 0x08},

	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_mode5_1280x720_raw10[] = {
	{0x3004, 0x03},
	{0x3005, 0x31},
	{0x3006, 0x00},
	{0x3007, 0xa9},

	{0x3018, 0xA2},  

	{0x306B, 0x05},
	{0x30E2, 0x03},

	{0x30EE, 0x01},
	{0x3342, 0x0A},
	{0x3343, 0x00},
	{0x3344, 0x1B},
	{0x3345, 0x00},
	{0x33A6, 0x01},
	{0x3528, 0x0E},
	{0x3554, 0x00},
	{0x3555, 0x01},
	{0x3556, 0x01},
	{0x3557, 0x01},
	{0x3558, 0x01},
	{0x3559, 0x00},
	{0x355A, 0x00},
	{0x35BA, 0x0E},
	{0x366A, 0x1B},
	{0x366B, 0x19},
	{0x366C, 0x17},
	{0x366D, 0x17},
	{0x3A41, 0x04},

	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_mode6_1280x540_raw10[] = {
	{0x3004, 0x04},  
	{0x3005, 0x31},
	{0x3006, 0x00},
	{0x3007, 0x02},  

	{0x3018, 0xA2},  

	{0x306B, 0x05},
	{0x30E2, 0x04},  

	{0x30EE, 0x01},
	{0x3342, 0x0A},
	{0x3343, 0x00},
	{0x3344, 0x16},
	{0x3345, 0x00},
	{0x33A6, 0x01},
	{0x3528, 0x0E},
	{0x3554, 0x1F},
	{0x3555, 0x01},
	{0x3556, 0x01},
	{0x3557, 0x01},
	{0x3558, 0x01},
	{0x3559, 0x00},
	{0x355A, 0x00},
	{0x35BA, 0x0E},
	{0x366A, 0x1B},
	{0x366B, 0x1A},
	{0x366C, 0x19},
	{0x366D, 0x17},
	{0x3A41, 0x04},

	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_start_1[] = {
	{IMX274_STANDBY_REG, 0x12},

	 
	{0x3120, 0xF0},
	{0x3121, 0x00},
	{0x3122, 0x02},
	{0x3129, 0x9C},
	{0x312A, 0x02},
	{0x312D, 0x02},

	{0x310B, 0x00},

	 
	{0x304C, 0x00},  
	{0x304D, 0x03},
	{0x331C, 0x1A},
	{0x331D, 0x00},
	{0x3502, 0x02},
	{0x3529, 0x0E},
	{0x352A, 0x0E},
	{0x352B, 0x0E},
	{0x3538, 0x0E},
	{0x3539, 0x0E},
	{0x3553, 0x00},
	{0x357D, 0x05},
	{0x357F, 0x05},
	{0x3581, 0x04},
	{0x3583, 0x76},
	{0x3587, 0x01},
	{0x35BB, 0x0E},
	{0x35BC, 0x0E},
	{0x35BD, 0x0E},
	{0x35BE, 0x0E},
	{0x35BF, 0x0E},
	{0x366E, 0x00},
	{0x366F, 0x00},
	{0x3670, 0x00},
	{0x3671, 0x00},

	 
	{0x3304, 0x32},  
	{0x3305, 0x00},
	{0x3306, 0x32},
	{0x3307, 0x00},
	{0x3590, 0x32},
	{0x3591, 0x00},
	{0x3686, 0x32},
	{0x3687, 0x00},

	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_start_2[] = {
	{IMX274_STANDBY_REG, 0x00},
	{0x303E, 0x02},  
	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_start_3[] = {
	{0x30F4, 0x00},
	{0x3018, 0xA2},  
	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_stop[] = {
	{IMX274_STANDBY_REG, 0x01},
	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_tp_disabled[] = {
	{0x303C, 0x00},
	{0x377F, 0x00},
	{0x3781, 0x00},
	{0x370B, 0x00},
	{IMX274_TABLE_END, 0x00}
};

 
static const struct reg_8 imx274_tp_regs[] = {
	{0x303C, 0x11},
	{0x370E, 0x01},
	{0x377F, 0x01},
	{0x3781, 0x01},
	{0x370B, 0x11},
	{IMX274_TABLE_END, 0x00}
};

 
static const struct imx274_mode imx274_modes[] = {
	{
		 
		.wbin_ratio = 1,  
		.hbin_ratio = 1,  
		.init_regs = imx274_mode1_3840x2160_raw10,
		.min_frame_len = 4550,
		.min_SHR = 12,
		.max_fps = 60,
		.nocpiop = 112,
	},
	{
		 
		.wbin_ratio = 2,  
		.hbin_ratio = 2,  
		.init_regs = imx274_mode3_1920x1080_raw10,
		.min_frame_len = 2310,
		.min_SHR = 8,
		.max_fps = 120,
		.nocpiop = 112,
	},
	{
		 
		.wbin_ratio = 3,  
		.hbin_ratio = 3,  
		.init_regs = imx274_mode5_1280x720_raw10,
		.min_frame_len = 2310,
		.min_SHR = 8,
		.max_fps = 120,
		.nocpiop = 112,
	},
	{
		 
		.wbin_ratio = 3,  
		.hbin_ratio = 4,  
		.init_regs = imx274_mode6_1280x540_raw10,
		.min_frame_len = 2310,
		.min_SHR = 4,
		.max_fps = 120,
		.nocpiop = 112,
	},
};

 
struct imx274_ctrls {
	struct v4l2_ctrl_handler handler;
	struct v4l2_ctrl *exposure;
	struct v4l2_ctrl *gain;
	struct v4l2_ctrl *vflip;
	struct v4l2_ctrl *test_pattern;
};

 
struct stimx274 {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct i2c_client *client;
	struct imx274_ctrls ctrls;
	struct v4l2_rect crop;
	struct v4l2_mbus_framefmt format;
	struct v4l2_fract frame_interval;
	struct regmap *regmap;
	struct gpio_desc *reset_gpio;
	struct regulator_bulk_data supplies[IMX274_NUM_SUPPLIES];
	struct clk *inck;
	struct mutex lock;  
	const struct imx274_mode *mode;
};

#define IMX274_ROUND(dim, step, flags)			\
	((flags) & V4L2_SEL_FLAG_GE			\
	 ? roundup((dim), (step))			\
	 : ((flags) & V4L2_SEL_FLAG_LE			\
	    ? rounddown((dim), (step))			\
	    : rounddown((dim) + (step) / 2, (step))))

 
static int imx274_set_gain(struct stimx274 *priv, struct v4l2_ctrl *ctrl);
static int imx274_set_exposure(struct stimx274 *priv, int val);
static int imx274_set_vflip(struct stimx274 *priv, int val);
static int imx274_set_test_pattern(struct stimx274 *priv, int val);
static int imx274_set_frame_interval(struct stimx274 *priv,
				     struct v4l2_fract frame_interval);

static inline void msleep_range(unsigned int delay_base)
{
	usleep_range(delay_base * 1000, delay_base * 1000 + 500);
}

 
static inline struct v4l2_subdev *ctrl_to_sd(struct v4l2_ctrl *ctrl)
{
	return &container_of(ctrl->handler,
			     struct stimx274, ctrls.handler)->sd;
}

static inline struct stimx274 *to_imx274(struct v4l2_subdev *sd)
{
	return container_of(sd, struct stimx274, sd);
}

 
static int imx274_write_table(struct stimx274 *priv, const struct reg_8 table[])
{
	struct regmap *regmap = priv->regmap;
	int err = 0;
	const struct reg_8 *next;
	u8 val;

	int range_start = -1;
	int range_count = 0;
	u8 range_vals[16];
	int max_range_vals = ARRAY_SIZE(range_vals);

	for (next = table;; next++) {
		if ((next->addr != range_start + range_count) ||
		    (next->addr == IMX274_TABLE_END) ||
		    (next->addr == IMX274_TABLE_WAIT_MS) ||
		    (range_count == max_range_vals)) {
			if (range_count == 1)
				err = regmap_write(regmap,
						   range_start, range_vals[0]);
			else if (range_count > 1)
				err = regmap_bulk_write(regmap, range_start,
							&range_vals[0],
							range_count);
			else
				err = 0;

			if (err)
				return err;

			range_start = -1;
			range_count = 0;

			 
			if (next->addr == IMX274_TABLE_END)
				break;

			if (next->addr == IMX274_TABLE_WAIT_MS) {
				msleep_range(next->val);
				continue;
			}
		}

		val = next->val;

		if (range_start == -1)
			range_start = next->addr;

		range_vals[range_count++] = val;
	}
	return 0;
}

static inline int imx274_write_reg(struct stimx274 *priv, u16 addr, u8 val)
{
	int err;

	err = regmap_write(priv->regmap, addr, val);
	if (err)
		dev_err(&priv->client->dev,
			"%s : i2c write failed, %x = %x\n", __func__,
			addr, val);
	else
		dev_dbg(&priv->client->dev,
			"%s : addr 0x%x, val=0x%x\n", __func__,
			addr, val);
	return err;
}

 
static int imx274_read_mbreg(struct stimx274 *priv, u16 addr, u32 *val,
			     size_t nbytes)
{
	__le32 val_le = 0;
	int err;

	err = regmap_bulk_read(priv->regmap, addr, &val_le, nbytes);
	if (err) {
		dev_err(&priv->client->dev,
			"%s : i2c bulk read failed, %x (%zu bytes)\n",
			__func__, addr, nbytes);
	} else {
		*val = le32_to_cpu(val_le);
		dev_dbg(&priv->client->dev,
			"%s : addr 0x%x, val=0x%x (%zu bytes)\n",
			__func__, addr, *val, nbytes);
	}

	return err;
}

 
static int imx274_write_mbreg(struct stimx274 *priv, u16 addr, u32 val,
			      size_t nbytes)
{
	__le32 val_le = cpu_to_le32(val);
	int err;

	err = regmap_bulk_write(priv->regmap, addr, &val_le, nbytes);
	if (err)
		dev_err(&priv->client->dev,
			"%s : i2c bulk write failed, %x = %x (%zu bytes)\n",
			__func__, addr, val, nbytes);
	else
		dev_dbg(&priv->client->dev,
			"%s : addr 0x%x, val=0x%x (%zu bytes)\n",
			__func__, addr, val, nbytes);
	return err;
}

 
static int imx274_mode_regs(struct stimx274 *priv)
{
	int err = 0;

	err = imx274_write_table(priv, imx274_start_1);
	if (err)
		return err;

	err = imx274_write_table(priv, priv->mode->init_regs);

	return err;
}

 
static int imx274_start_stream(struct stimx274 *priv)
{
	int err = 0;

	err = __v4l2_ctrl_handler_setup(&priv->ctrls.handler);
	if (err) {
		dev_err(&priv->client->dev, "Error %d setup controls\n", err);
		return err;
	}

	 
	msleep_range(11);
	err = imx274_write_table(priv, imx274_start_2);
	if (err)
		return err;

	 
	msleep_range(8);
	err = imx274_write_table(priv, imx274_start_3);
	if (err)
		return err;

	return 0;
}

 
static void imx274_reset(struct stimx274 *priv, int rst)
{
	gpiod_set_value_cansleep(priv->reset_gpio, 0);
	usleep_range(IMX274_RESET_DELAY1, IMX274_RESET_DELAY2);
	gpiod_set_value_cansleep(priv->reset_gpio, !!rst);
	usleep_range(IMX274_RESET_DELAY1, IMX274_RESET_DELAY2);
}

static int imx274_power_on(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct stimx274 *imx274 = to_imx274(sd);
	int ret;

	 
	imx274_reset(imx274, 0);

	ret = clk_prepare_enable(imx274->inck);
	if (ret) {
		dev_err(&imx274->client->dev,
			"Failed to enable input clock: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(IMX274_NUM_SUPPLIES, imx274->supplies);
	if (ret) {
		dev_err(&imx274->client->dev,
			"Failed to enable regulators: %d\n", ret);
		goto fail_reg;
	}

	udelay(2);
	imx274_reset(imx274, 1);

	return 0;

fail_reg:
	clk_disable_unprepare(imx274->inck);
	return ret;
}

static int imx274_power_off(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct stimx274 *imx274 = to_imx274(sd);

	imx274_reset(imx274, 0);

	regulator_bulk_disable(IMX274_NUM_SUPPLIES, imx274->supplies);

	clk_disable_unprepare(imx274->inck);

	return 0;
}

static int imx274_regulators_get(struct device *dev, struct stimx274 *imx274)
{
	unsigned int i;

	for (i = 0; i < IMX274_NUM_SUPPLIES; i++)
		imx274->supplies[i].supply = imx274_supply_names[i];

	return devm_regulator_bulk_get(dev, IMX274_NUM_SUPPLIES,
					imx274->supplies);
}

 
static int imx274_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd = ctrl_to_sd(ctrl);
	struct stimx274 *imx274 = to_imx274(sd);
	int ret = -EINVAL;

	if (!pm_runtime_get_if_in_use(&imx274->client->dev))
		return 0;

	dev_dbg(&imx274->client->dev,
		"%s : s_ctrl: %s, value: %d\n", __func__,
		ctrl->name, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_EXPOSURE:
		dev_dbg(&imx274->client->dev,
			"%s : set V4L2_CID_EXPOSURE\n", __func__);
		ret = imx274_set_exposure(imx274, ctrl->val);
		break;

	case V4L2_CID_GAIN:
		dev_dbg(&imx274->client->dev,
			"%s : set V4L2_CID_GAIN\n", __func__);
		ret = imx274_set_gain(imx274, ctrl);
		break;

	case V4L2_CID_VFLIP:
		dev_dbg(&imx274->client->dev,
			"%s : set V4L2_CID_VFLIP\n", __func__);
		ret = imx274_set_vflip(imx274, ctrl->val);
		break;

	case V4L2_CID_TEST_PATTERN:
		dev_dbg(&imx274->client->dev,
			"%s : set V4L2_CID_TEST_PATTERN\n", __func__);
		ret = imx274_set_test_pattern(imx274, ctrl->val);
		break;
	}

	pm_runtime_put(&imx274->client->dev);

	return ret;
}

static int imx274_binning_goodness(struct stimx274 *imx274,
				   int w, int ask_w,
				   int h, int ask_h, u32 flags)
{
	struct device *dev = &imx274->client->dev;
	const int goodness = 100000;
	int val = 0;

	if (flags & V4L2_SEL_FLAG_GE) {
		if (w < ask_w)
			val -= goodness;
		if (h < ask_h)
			val -= goodness;
	}

	if (flags & V4L2_SEL_FLAG_LE) {
		if (w > ask_w)
			val -= goodness;
		if (h > ask_h)
			val -= goodness;
	}

	val -= abs(w - ask_w);
	val -= abs(h - ask_h);

	dev_dbg(dev, "%s: ask %dx%d, size %dx%d, goodness %d\n",
		__func__, ask_w, ask_h, w, h, val);

	return val;
}

 
static int __imx274_change_compose(struct stimx274 *imx274,
				   struct v4l2_subdev_state *sd_state,
				   u32 which,
				   u32 *width,
				   u32 *height,
				   u32 flags)
{
	struct device *dev = &imx274->client->dev;
	const struct v4l2_rect *cur_crop;
	struct v4l2_mbus_framefmt *tgt_fmt;
	unsigned int i;
	const struct imx274_mode *best_mode = &imx274_modes[0];
	int best_goodness = INT_MIN;

	if (which == V4L2_SUBDEV_FORMAT_TRY) {
		cur_crop = &sd_state->pads->try_crop;
		tgt_fmt = &sd_state->pads->try_fmt;
	} else {
		cur_crop = &imx274->crop;
		tgt_fmt = &imx274->format;
	}

	for (i = 0; i < ARRAY_SIZE(imx274_modes); i++) {
		u8 wratio = imx274_modes[i].wbin_ratio;
		u8 hratio = imx274_modes[i].hbin_ratio;

		int goodness = imx274_binning_goodness(
			imx274,
			cur_crop->width / wratio, *width,
			cur_crop->height / hratio, *height,
			flags);

		if (goodness >= best_goodness) {
			best_goodness = goodness;
			best_mode = &imx274_modes[i];
		}
	}

	*width = cur_crop->width / best_mode->wbin_ratio;
	*height = cur_crop->height / best_mode->hbin_ratio;

	if (which == V4L2_SUBDEV_FORMAT_ACTIVE)
		imx274->mode = best_mode;

	dev_dbg(dev, "%s: selected %ux%u binning\n",
		__func__, best_mode->wbin_ratio, best_mode->hbin_ratio);

	tgt_fmt->width = *width;
	tgt_fmt->height = *height;
	tgt_fmt->field = V4L2_FIELD_NONE;

	return 0;
}

 
static int imx274_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *fmt)
{
	struct stimx274 *imx274 = to_imx274(sd);

	mutex_lock(&imx274->lock);
	fmt->format = imx274->format;
	mutex_unlock(&imx274->lock);
	return 0;
}

 
static int imx274_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_state *sd_state,
			  struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *fmt = &format->format;
	struct stimx274 *imx274 = to_imx274(sd);
	int err = 0;

	mutex_lock(&imx274->lock);

	err = __imx274_change_compose(imx274, sd_state, format->which,
				      &fmt->width, &fmt->height, 0);

	if (err)
		goto out;

	 
	fmt->field = V4L2_FIELD_NONE;
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		sd_state->pads->try_fmt = *fmt;
	else
		imx274->format = *fmt;

out:
	mutex_unlock(&imx274->lock);

	return err;
}

static int imx274_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct stimx274 *imx274 = to_imx274(sd);
	const struct v4l2_rect *src_crop;
	const struct v4l2_mbus_framefmt *src_fmt;
	int ret = 0;

	if (sel->pad != 0)
		return -EINVAL;

	if (sel->target == V4L2_SEL_TGT_CROP_BOUNDS) {
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = IMX274_MAX_WIDTH;
		sel->r.height = IMX274_MAX_HEIGHT;
		return 0;
	}

	if (sel->which == V4L2_SUBDEV_FORMAT_TRY) {
		src_crop = &sd_state->pads->try_crop;
		src_fmt = &sd_state->pads->try_fmt;
	} else {
		src_crop = &imx274->crop;
		src_fmt = &imx274->format;
	}

	mutex_lock(&imx274->lock);

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP:
		sel->r = *src_crop;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = src_crop->width;
		sel->r.height = src_crop->height;
		break;
	case V4L2_SEL_TGT_COMPOSE:
		sel->r.top = 0;
		sel->r.left = 0;
		sel->r.width = src_fmt->width;
		sel->r.height = src_fmt->height;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&imx274->lock);

	return ret;
}

static int imx274_set_selection_crop(struct stimx274 *imx274,
				     struct v4l2_subdev_state *sd_state,
				     struct v4l2_subdev_selection *sel)
{
	struct v4l2_rect *tgt_crop;
	struct v4l2_rect new_crop;
	bool size_changed;

	 
	const u32 h_step = 24;

	new_crop.width = min_t(u32,
			       IMX274_ROUND(sel->r.width, h_step, sel->flags),
			       IMX274_MAX_WIDTH);

	 
	if (new_crop.width < 144)
		new_crop.width = 144;

	new_crop.left = min_t(u32,
			      IMX274_ROUND(sel->r.left, h_step, 0),
			      IMX274_MAX_WIDTH - new_crop.width);

	new_crop.height = min_t(u32,
				IMX274_ROUND(sel->r.height, 2, sel->flags),
				IMX274_MAX_HEIGHT);

	new_crop.top = min_t(u32, IMX274_ROUND(sel->r.top, 2, 0),
			     IMX274_MAX_HEIGHT - new_crop.height);

	sel->r = new_crop;

	if (sel->which == V4L2_SUBDEV_FORMAT_TRY)
		tgt_crop = &sd_state->pads->try_crop;
	else
		tgt_crop = &imx274->crop;

	mutex_lock(&imx274->lock);

	size_changed = (new_crop.width != tgt_crop->width ||
			new_crop.height != tgt_crop->height);

	 
	*tgt_crop = new_crop;

	 
	if (size_changed)
		__imx274_change_compose(imx274, sd_state, sel->which,
					&new_crop.width, &new_crop.height,
					sel->flags);

	mutex_unlock(&imx274->lock);

	return 0;
}

static int imx274_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_state *sd_state,
				struct v4l2_subdev_selection *sel)
{
	struct stimx274 *imx274 = to_imx274(sd);

	if (sel->pad != 0)
		return -EINVAL;

	if (sel->target == V4L2_SEL_TGT_CROP)
		return imx274_set_selection_crop(imx274, sd_state, sel);

	if (sel->target == V4L2_SEL_TGT_COMPOSE) {
		int err;

		mutex_lock(&imx274->lock);
		err =  __imx274_change_compose(imx274, sd_state, sel->which,
					       &sel->r.width, &sel->r.height,
					       sel->flags);
		mutex_unlock(&imx274->lock);

		 
		if (!err) {
			sel->r.top = 0;
			sel->r.left = 0;
		}

		return err;
	}

	return -EINVAL;
}

static int imx274_apply_trimming(struct stimx274 *imx274)
{
	u32 h_start;
	u32 h_end;
	u32 hmax;
	u32 v_cut;
	s32 v_pos;
	u32 write_v_size;
	u32 y_out_size;
	int err;

	h_start = imx274->crop.left + 12;
	h_end = h_start + imx274->crop.width;

	 
	 
	 
	hmax = max_t(u32, 260, (imx274->crop.width) / 16 + 23);

	 
	v_pos = imx274->ctrls.vflip->cur.val ?
		(-imx274->crop.top / 2) : (imx274->crop.top / 2);
	v_cut = (IMX274_MAX_HEIGHT - imx274->crop.height) / 2;
	write_v_size = imx274->crop.height + 22;
	y_out_size   = imx274->crop.height;

	err = imx274_write_mbreg(imx274, IMX274_HMAX_REG_LSB, hmax, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_HTRIM_EN_REG, 1, 1);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_HTRIM_START_REG_LSB,
					 h_start, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_HTRIM_END_REG_LSB,
					 h_end, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_VWIDCUTEN_REG, 1, 1);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_VWIDCUT_REG_LSB,
					 v_cut, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_VWINPOS_REG_LSB,
					 v_pos, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_WRITE_VSIZE_REG_LSB,
					 write_v_size, 2);
	if (!err)
		err = imx274_write_mbreg(imx274, IMX274_Y_OUT_SIZE_REG_LSB,
					 y_out_size, 2);

	return err;
}

 
static int imx274_g_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct stimx274 *imx274 = to_imx274(sd);

	fi->interval = imx274->frame_interval;
	dev_dbg(&imx274->client->dev, "%s frame rate = %d / %d\n",
		__func__, imx274->frame_interval.numerator,
		imx274->frame_interval.denominator);

	return 0;
}

 
static int imx274_s_frame_interval(struct v4l2_subdev *sd,
				   struct v4l2_subdev_frame_interval *fi)
{
	struct stimx274 *imx274 = to_imx274(sd);
	struct v4l2_ctrl *ctrl = imx274->ctrls.exposure;
	int min, max, def;
	int ret;

	ret = pm_runtime_resume_and_get(&imx274->client->dev);
	if (ret < 0)
		return ret;

	mutex_lock(&imx274->lock);
	ret = imx274_set_frame_interval(imx274, fi->interval);

	if (!ret) {
		fi->interval = imx274->frame_interval;

		 
		min = IMX274_MIN_EXPOSURE_TIME;
		max = fi->interval.numerator * 1000000
			/ fi->interval.denominator;
		def = max;
		ret = __v4l2_ctrl_modify_range(ctrl, min, max, 1, def);
		if (ret) {
			dev_err(&imx274->client->dev,
				"Exposure ctrl range update failed\n");
			goto unlock;
		}

		 
		imx274_set_exposure(imx274, ctrl->val);

		dev_dbg(&imx274->client->dev, "set frame interval to %uus\n",
			fi->interval.numerator * 1000000
			/ fi->interval.denominator);
	}

unlock:
	mutex_unlock(&imx274->lock);
	pm_runtime_put(&imx274->client->dev);

	return ret;
}

 
static void imx274_load_default(struct stimx274 *priv)
{
	 
	priv->frame_interval.numerator = 1;
	priv->frame_interval.denominator = IMX274_DEF_FRAME_RATE;
	priv->ctrls.exposure->val = 1000000 / IMX274_DEF_FRAME_RATE;
	priv->ctrls.gain->val = IMX274_DEF_GAIN;
	priv->ctrls.vflip->val = 0;
	priv->ctrls.test_pattern->val = TEST_PATTERN_DISABLED;
}

 
static int imx274_s_stream(struct v4l2_subdev *sd, int on)
{
	struct stimx274 *imx274 = to_imx274(sd);
	int ret = 0;

	dev_dbg(&imx274->client->dev, "%s : %s, mode index = %td\n", __func__,
		on ? "Stream Start" : "Stream Stop",
		imx274->mode - &imx274_modes[0]);

	mutex_lock(&imx274->lock);

	if (on) {
		ret = pm_runtime_resume_and_get(&imx274->client->dev);
		if (ret < 0) {
			mutex_unlock(&imx274->lock);
			return ret;
		}

		 
		ret = imx274_mode_regs(imx274);
		if (ret)
			goto fail;

		ret = imx274_apply_trimming(imx274);
		if (ret)
			goto fail;

		 
		ret = imx274_set_frame_interval(imx274,
						imx274->frame_interval);
		if (ret)
			goto fail;

		 
		ret = imx274_start_stream(imx274);
		if (ret)
			goto fail;
	} else {
		 
		ret = imx274_write_table(imx274, imx274_stop);
		if (ret)
			goto fail;

		pm_runtime_put(&imx274->client->dev);
	}

	mutex_unlock(&imx274->lock);
	dev_dbg(&imx274->client->dev, "%s : Done\n", __func__);
	return 0;

fail:
	pm_runtime_put(&imx274->client->dev);
	mutex_unlock(&imx274->lock);
	dev_err(&imx274->client->dev, "s_stream failed\n");
	return ret;
}

 
static int imx274_get_frame_length(struct stimx274 *priv, u32 *val)
{
	int err;
	u32 svr;
	u32 vmax;

	err = imx274_read_mbreg(priv, IMX274_SVR_REG_LSB, &svr, 2);
	if (err)
		goto fail;

	err = imx274_read_mbreg(priv, IMX274_VMAX_REG_3, &vmax, 3);
	if (err)
		goto fail;

	*val = vmax * (svr + 1);

	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
	return err;
}

static int imx274_clamp_coarse_time(struct stimx274 *priv, u32 *val,
				    u32 *frame_length)
{
	int err;

	err = imx274_get_frame_length(priv, frame_length);
	if (err)
		return err;

	if (*frame_length < priv->mode->min_frame_len)
		*frame_length =  priv->mode->min_frame_len;

	*val = *frame_length - *val;  
	if (*val > *frame_length - IMX274_SHR_LIMIT_CONST)
		*val = *frame_length - IMX274_SHR_LIMIT_CONST;
	else if (*val < priv->mode->min_SHR)
		*val = priv->mode->min_SHR;

	return 0;
}

 
static int imx274_set_digital_gain(struct stimx274 *priv, u32 dgain)
{
	u8 reg_val;

	reg_val = ffs(dgain);

	if (reg_val)
		reg_val--;

	reg_val = clamp(reg_val, (u8)0, (u8)3);

	return imx274_write_reg(priv, IMX274_DIGITAL_GAIN_REG,
				reg_val & IMX274_MASK_LSB_4_BITS);
}

 
static int imx274_set_gain(struct stimx274 *priv, struct v4l2_ctrl *ctrl)
{
	int err;
	u32 gain, analog_gain, digital_gain, gain_reg;

	gain = (u32)(ctrl->val);

	dev_dbg(&priv->client->dev,
		"%s : input gain = %d.%d\n", __func__,
		gain >> IMX274_GAIN_SHIFT,
		((gain & IMX274_GAIN_SHIFT_MASK) * 100) >> IMX274_GAIN_SHIFT);

	if (gain > IMX274_MAX_DIGITAL_GAIN * IMX274_MAX_ANALOG_GAIN)
		gain = IMX274_MAX_DIGITAL_GAIN * IMX274_MAX_ANALOG_GAIN;
	else if (gain < IMX274_MIN_GAIN)
		gain = IMX274_MIN_GAIN;

	if (gain <= IMX274_MAX_ANALOG_GAIN)
		digital_gain = 1;
	else if (gain <= IMX274_MAX_ANALOG_GAIN * 2)
		digital_gain = 2;
	else if (gain <= IMX274_MAX_ANALOG_GAIN * 4)
		digital_gain = 4;
	else
		digital_gain = IMX274_MAX_DIGITAL_GAIN;

	analog_gain = gain / digital_gain;

	dev_dbg(&priv->client->dev,
		"%s : digital gain = %d, analog gain = %d.%d\n",
		__func__, digital_gain, analog_gain >> IMX274_GAIN_SHIFT,
		((analog_gain & IMX274_GAIN_SHIFT_MASK) * 100)
		>> IMX274_GAIN_SHIFT);

	err = imx274_set_digital_gain(priv, digital_gain);
	if (err)
		goto fail;

	 
	gain_reg = (u32)IMX274_GAIN_CONST -
		(IMX274_GAIN_CONST << IMX274_GAIN_SHIFT) / analog_gain;
	if (gain_reg > IMX274_GAIN_REG_MAX)
		gain_reg = IMX274_GAIN_REG_MAX;

	err = imx274_write_mbreg(priv, IMX274_ANALOG_GAIN_ADDR_LSB, gain_reg,
				 2);
	if (err)
		goto fail;

	if (IMX274_GAIN_CONST - gain_reg == 0) {
		err = -EINVAL;
		goto fail;
	}

	 
	ctrl->val = (IMX274_GAIN_CONST << IMX274_GAIN_SHIFT)
			/ (IMX274_GAIN_CONST - gain_reg) * digital_gain;

	dev_dbg(&priv->client->dev,
		"%s : GAIN control success, gain_reg = %d, new gain = %d\n",
		__func__, gain_reg, ctrl->val);

	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
	return err;
}

 
static int imx274_set_coarse_time(struct stimx274 *priv, u32 *val)
{
	int err;
	u32 coarse_time, frame_length;

	coarse_time = *val;

	 
	err = imx274_clamp_coarse_time(priv, &coarse_time, &frame_length);
	if (err)
		goto fail;

	err = imx274_write_mbreg(priv, IMX274_SHR_REG_LSB, coarse_time, 2);
	if (err)
		goto fail;

	*val = frame_length - coarse_time;
	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
	return err;
}

 
static int imx274_set_exposure(struct stimx274 *priv, int val)
{
	int err;
	u32 hmax;
	u32 coarse_time;  

	dev_dbg(&priv->client->dev,
		"%s : EXPOSURE control input = %d\n", __func__, val);

	 

	err = imx274_read_mbreg(priv, IMX274_HMAX_REG_LSB, &hmax, 2);
	if (err)
		goto fail;

	if (hmax == 0) {
		err = -EINVAL;
		goto fail;
	}

	coarse_time = (IMX274_PIXCLK_CONST1 / IMX274_PIXCLK_CONST2 * val
			- priv->mode->nocpiop) / hmax;

	 

	 
	err = imx274_set_coarse_time(priv, &coarse_time);
	if (err)
		goto fail;

	priv->ctrls.exposure->val =
			(coarse_time * hmax + priv->mode->nocpiop)
			/ (IMX274_PIXCLK_CONST1 / IMX274_PIXCLK_CONST2);

	dev_dbg(&priv->client->dev,
		"%s : EXPOSURE control success\n", __func__);
	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);

	return err;
}

 
static int imx274_set_vflip(struct stimx274 *priv, int val)
{
	int err;

	err = imx274_write_reg(priv, IMX274_VFLIP_REG, val);
	if (err) {
		dev_err(&priv->client->dev, "VFLIP control error\n");
		return err;
	}

	dev_dbg(&priv->client->dev,
		"%s : VFLIP control success\n", __func__);

	return 0;
}

 
static int imx274_set_test_pattern(struct stimx274 *priv, int val)
{
	int err = 0;

	if (val == TEST_PATTERN_DISABLED) {
		err = imx274_write_table(priv, imx274_tp_disabled);
	} else if (val <= TEST_PATTERN_V_COLOR_BARS) {
		err = imx274_write_reg(priv, IMX274_TEST_PATTERN_REG, val - 1);
		if (!err)
			err = imx274_write_table(priv, imx274_tp_regs);
	} else {
		err = -EINVAL;
	}

	if (!err)
		dev_dbg(&priv->client->dev,
			"%s : TEST PATTERN control success\n", __func__);
	else
		dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);

	return err;
}

 
static int imx274_set_frame_length(struct stimx274 *priv, u32 val)
{
	int err;
	u32 frame_length;

	dev_dbg(&priv->client->dev, "%s : input length = %d\n",
		__func__, val);

	frame_length = (u32)val;

	err = imx274_write_mbreg(priv, IMX274_VMAX_REG_3, frame_length, 3);
	if (err)
		goto fail;

	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
	return err;
}

 
static int imx274_set_frame_interval(struct stimx274 *priv,
				     struct v4l2_fract frame_interval)
{
	int err;
	u32 frame_length, req_frame_rate;
	u32 svr;
	u32 hmax;

	dev_dbg(&priv->client->dev, "%s: input frame interval = %d / %d",
		__func__, frame_interval.numerator,
		frame_interval.denominator);

	if (frame_interval.numerator == 0 || frame_interval.denominator == 0) {
		frame_interval.denominator = IMX274_DEF_FRAME_RATE;
		frame_interval.numerator = 1;
	}

	req_frame_rate = (u32)(frame_interval.denominator
				/ frame_interval.numerator);

	 
	if (req_frame_rate > priv->mode->max_fps) {
		frame_interval.numerator = 1;
		frame_interval.denominator = priv->mode->max_fps;
	} else if (req_frame_rate < IMX274_MIN_FRAME_RATE) {
		frame_interval.numerator = 1;
		frame_interval.denominator = IMX274_MIN_FRAME_RATE;
	}

	 

	err = imx274_read_mbreg(priv, IMX274_SVR_REG_LSB, &svr, 2);
	if (err)
		goto fail;

	dev_dbg(&priv->client->dev,
		"%s : register SVR = %d\n", __func__, svr);

	err = imx274_read_mbreg(priv, IMX274_HMAX_REG_LSB, &hmax, 2);
	if (err)
		goto fail;

	dev_dbg(&priv->client->dev,
		"%s : register HMAX = %d\n", __func__, hmax);

	if (hmax == 0 || frame_interval.denominator == 0) {
		err = -EINVAL;
		goto fail;
	}

	frame_length = IMX274_PIXCLK_CONST1 / (svr + 1) / hmax
					* frame_interval.numerator
					/ frame_interval.denominator;

	err = imx274_set_frame_length(priv, frame_length);
	if (err)
		goto fail;

	priv->frame_interval = frame_interval;
	return 0;

fail:
	dev_err(&priv->client->dev, "%s error = %d\n", __func__, err);
	return err;
}

static int imx274_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index > 0)
		return -EINVAL;

	 
	code->code = MEDIA_BUS_FMT_SRGGB10_1X10;

	return 0;
}

static const struct v4l2_subdev_pad_ops imx274_pad_ops = {
	.enum_mbus_code = imx274_enum_mbus_code,
	.get_fmt = imx274_get_fmt,
	.set_fmt = imx274_set_fmt,
	.get_selection = imx274_get_selection,
	.set_selection = imx274_set_selection,
};

static const struct v4l2_subdev_video_ops imx274_video_ops = {
	.g_frame_interval = imx274_g_frame_interval,
	.s_frame_interval = imx274_s_frame_interval,
	.s_stream = imx274_s_stream,
};

static const struct v4l2_subdev_ops imx274_subdev_ops = {
	.pad = &imx274_pad_ops,
	.video = &imx274_video_ops,
};

static const struct v4l2_ctrl_ops imx274_ctrl_ops = {
	.s_ctrl	= imx274_s_ctrl,
};

static const struct of_device_id imx274_of_id_table[] = {
	{ .compatible = "sony,imx274" },
	{ }
};
MODULE_DEVICE_TABLE(of, imx274_of_id_table);

static const struct i2c_device_id imx274_id[] = {
	{ "IMX274", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx274_id);

static int imx274_fwnode_parse(struct device *dev)
{
	struct fwnode_handle *endpoint;
	 
	struct v4l2_fwnode_endpoint ep = {
		.bus_type = V4L2_MBUS_CSI2_DPHY
	};
	int ret;

	endpoint = fwnode_graph_get_next_endpoint(dev_fwnode(dev), NULL);
	if (!endpoint) {
		dev_err(dev, "Endpoint node not found\n");
		return -EINVAL;
	}

	ret = v4l2_fwnode_endpoint_parse(endpoint, &ep);
	fwnode_handle_put(endpoint);
	if (ret == -ENXIO) {
		dev_err(dev, "Unsupported bus type, should be CSI2\n");
		return ret;
	} else if (ret) {
		dev_err(dev, "Parsing endpoint node failed %d\n", ret);
		return ret;
	}

	 
	if (ep.bus.mipi_csi2.num_data_lanes != 4) {
		dev_err(dev, "Invalid data lanes: %d\n",
			ep.bus.mipi_csi2.num_data_lanes);
		return -EINVAL;
	}

	return 0;
}

static int imx274_probe(struct i2c_client *client)
{
	struct v4l2_subdev *sd;
	struct stimx274 *imx274;
	struct device *dev = &client->dev;
	int ret;

	 
	imx274 = devm_kzalloc(dev, sizeof(*imx274), GFP_KERNEL);
	if (!imx274)
		return -ENOMEM;

	mutex_init(&imx274->lock);

	ret = imx274_fwnode_parse(dev);
	if (ret)
		return ret;

	imx274->inck = devm_clk_get_optional(dev, "inck");
	if (IS_ERR(imx274->inck))
		return PTR_ERR(imx274->inck);

	ret = imx274_regulators_get(dev, imx274);
	if (ret) {
		dev_err(dev, "Failed to get power regulators, err: %d\n", ret);
		return ret;
	}

	 
	imx274->mode = &imx274_modes[0];
	imx274->crop.width = IMX274_MAX_WIDTH;
	imx274->crop.height = IMX274_MAX_HEIGHT;
	imx274->format.width = imx274->crop.width / imx274->mode->wbin_ratio;
	imx274->format.height = imx274->crop.height / imx274->mode->hbin_ratio;
	imx274->format.field = V4L2_FIELD_NONE;
	imx274->format.code = MEDIA_BUS_FMT_SRGGB10_1X10;
	imx274->format.colorspace = V4L2_COLORSPACE_SRGB;
	imx274->frame_interval.numerator = 1;
	imx274->frame_interval.denominator = IMX274_DEF_FRAME_RATE;

	 
	imx274->regmap = devm_regmap_init_i2c(client, &imx274_regmap_config);
	if (IS_ERR(imx274->regmap)) {
		dev_err(dev,
			"regmap init failed: %ld\n", PTR_ERR(imx274->regmap));
		ret = -ENODEV;
		goto err_regmap;
	}

	 
	imx274->client = client;
	sd = &imx274->sd;
	v4l2_i2c_subdev_init(sd, client, &imx274_subdev_ops);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS;

	 
	imx274->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.function = MEDIA_ENT_F_CAM_SENSOR;
	ret = media_entity_pads_init(&sd->entity, 1, &imx274->pad);
	if (ret < 0) {
		dev_err(dev,
			"%s : media entity init Failed %d\n", __func__, ret);
		goto err_regmap;
	}

	 
	imx274->reset_gpio = devm_gpiod_get_optional(dev, "reset",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(imx274->reset_gpio)) {
		ret = dev_err_probe(dev, PTR_ERR(imx274->reset_gpio),
				    "Reset GPIO not setup in DT\n");
		goto err_me;
	}

	 
	ret = imx274_power_on(dev);
	if (ret < 0) {
		dev_err(dev, "%s : imx274 power on failed\n", __func__);
		goto err_me;
	}

	 
	ret = v4l2_ctrl_handler_init(&imx274->ctrls.handler, 4);
	if (ret < 0) {
		dev_err(dev, "%s : ctrl handler init Failed\n", __func__);
		goto err_power_off;
	}

	imx274->ctrls.handler.lock = &imx274->lock;

	 
	imx274->ctrls.test_pattern = v4l2_ctrl_new_std_menu_items(
		&imx274->ctrls.handler, &imx274_ctrl_ops,
		V4L2_CID_TEST_PATTERN,
		ARRAY_SIZE(tp_qmenu) - 1, 0, 0, tp_qmenu);

	imx274->ctrls.gain = v4l2_ctrl_new_std(
		&imx274->ctrls.handler,
		&imx274_ctrl_ops,
		V4L2_CID_GAIN, IMX274_MIN_GAIN,
		IMX274_MAX_DIGITAL_GAIN * IMX274_MAX_ANALOG_GAIN, 1,
		IMX274_DEF_GAIN);

	imx274->ctrls.exposure = v4l2_ctrl_new_std(
		&imx274->ctrls.handler,
		&imx274_ctrl_ops,
		V4L2_CID_EXPOSURE, IMX274_MIN_EXPOSURE_TIME,
		1000000 / IMX274_DEF_FRAME_RATE, 1,
		IMX274_MIN_EXPOSURE_TIME);

	imx274->ctrls.vflip = v4l2_ctrl_new_std(
		&imx274->ctrls.handler,
		&imx274_ctrl_ops,
		V4L2_CID_VFLIP, 0, 1, 1, 0);

	imx274->sd.ctrl_handler = &imx274->ctrls.handler;
	if (imx274->ctrls.handler.error) {
		ret = imx274->ctrls.handler.error;
		goto err_ctrls;
	}

	 
	imx274_load_default(imx274);

	 
	ret = v4l2_async_register_subdev(sd);
	if (ret < 0) {
		dev_err(dev, "%s : v4l2_async_register_subdev failed %d\n",
			__func__, ret);
		goto err_ctrls;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

	dev_info(dev, "imx274 : imx274 probe success !\n");
	return 0;

err_ctrls:
	v4l2_ctrl_handler_free(&imx274->ctrls.handler);
err_power_off:
	imx274_power_off(dev);
err_me:
	media_entity_cleanup(&sd->entity);
err_regmap:
	mutex_destroy(&imx274->lock);
	return ret;
}

static void imx274_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct stimx274 *imx274 = to_imx274(sd);

	pm_runtime_disable(&client->dev);
	if (!pm_runtime_status_suspended(&client->dev))
		imx274_power_off(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	v4l2_async_unregister_subdev(sd);
	v4l2_ctrl_handler_free(&imx274->ctrls.handler);

	media_entity_cleanup(&sd->entity);
	mutex_destroy(&imx274->lock);
}

static const struct dev_pm_ops imx274_pm_ops = {
	SET_RUNTIME_PM_OPS(imx274_power_off, imx274_power_on, NULL)
};

static struct i2c_driver imx274_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.pm = &imx274_pm_ops,
		.of_match_table	= imx274_of_id_table,
	},
	.probe		= imx274_probe,
	.remove		= imx274_remove,
	.id_table	= imx274_id,
};

module_i2c_driver(imx274_i2c_driver);

MODULE_AUTHOR("Leon Luo <leonl@leopardimaging.com>");
MODULE_DESCRIPTION("IMX274 CMOS Image Sensor driver");
MODULE_LICENSE("GPL v2");
