
 

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>

#include <media/i2c/mt9t112.h>
#include <media/v4l2-common.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-subdev.h>

 
 

 
 
#define MAX_WIDTH   2048
#define MAX_HEIGHT  1536

 
#define ECHECKER(ret, x)		\
	do {				\
		(ret) = (x);		\
		if ((ret) < 0)		\
			return (ret);	\
	} while (0)

#define mt9t112_reg_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_reg_write(client, a, b))
#define mt9t112_mcu_write(ret, client, a, b) \
	ECHECKER(ret, __mt9t112_mcu_write(client, a, b))

#define mt9t112_reg_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_reg_mask_set(client, a, b, c))
#define mt9t112_mcu_mask_set(ret, client, a, b, c) \
	ECHECKER(ret, __mt9t112_mcu_mask_set(client, a, b, c))

#define mt9t112_reg_read(ret, client, a) \
	ECHECKER(ret, __mt9t112_reg_read(client, a))

 
#define _VAR(id, offset, base)	(base | (id & 0x1f) << 10 | (offset & 0x3ff))
#define VAR(id, offset)  _VAR(id, offset, 0x0000)
#define VAR8(id, offset) _VAR(id, offset, 0x8000)

 
struct mt9t112_format {
	u32 code;
	enum v4l2_colorspace colorspace;
	u16 fmt;
	u16 order;
};

struct mt9t112_priv {
	struct v4l2_subdev		 subdev;
	struct mt9t112_platform_data	*info;
	struct i2c_client		*client;
	struct v4l2_rect		 frame;
	struct clk			*clk;
	struct gpio_desc		*standby_gpio;
	const struct mt9t112_format	*format;
	int				 num_formats;
	bool				 init_done;
};

 

static const struct mt9t112_format mt9t112_cfmts[] = {
	{
		.code		= MEDIA_BUS_FMT_UYVY8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 1,
		.order		= 0,
	}, {
		.code		= MEDIA_BUS_FMT_VYUY8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 1,
		.order		= 1,
	}, {
		.code		= MEDIA_BUS_FMT_YUYV8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 1,
		.order		= 2,
	}, {
		.code		= MEDIA_BUS_FMT_YVYU8_2X8,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 1,
		.order		= 3,
	}, {
		.code		= MEDIA_BUS_FMT_RGB555_2X8_PADHI_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 8,
		.order		= 2,
	}, {
		.code		= MEDIA_BUS_FMT_RGB565_2X8_LE,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.fmt		= 4,
		.order		= 2,
	},
};

 
static struct mt9t112_priv *to_mt9t112(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client),
			    struct mt9t112_priv,
			    subdev);
}

static int __mt9t112_reg_read(const struct i2c_client *client, u16 command)
{
	struct i2c_msg msg[2];
	u8 buf[2];
	int ret;

	command = swab16(command);

	msg[0].addr  = client->addr;
	msg[0].flags = 0;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *)&command;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = 2;
	msg[1].buf   = buf;

	 
	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	memcpy(&ret, buf, 2);

	return swab16(ret);
}

static int __mt9t112_reg_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	struct i2c_msg msg;
	u8 buf[4];
	int ret;

	command = swab16(command);
	data = swab16(data);

	memcpy(buf + 0, &command, 2);
	memcpy(buf + 2, &data,    2);

	msg.addr  = client->addr;
	msg.flags = 0;
	msg.len   = 4;
	msg.buf   = buf;

	 
	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret >= 0 ? 0 : ret;
}

static int __mt9t112_reg_mask_set(const struct i2c_client *client,
				  u16  command, u16  mask, u16  set)
{
	int val = __mt9t112_reg_read(client, command);

	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_reg_write(client, command, val);
}

 
static int __mt9t112_mcu_read(const struct i2c_client *client, u16 command)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_read(client, 0x0990);
}

static int __mt9t112_mcu_write(const struct i2c_client *client,
			       u16 command, u16 data)
{
	int ret;

	ret = __mt9t112_reg_write(client, 0x098E, command);
	if (ret < 0)
		return ret;

	return __mt9t112_reg_write(client, 0x0990, data);
}

static int __mt9t112_mcu_mask_set(const struct i2c_client *client,
				  u16  command, u16  mask, u16  set)
{
	int val = __mt9t112_mcu_read(client, command);

	if (val < 0)
		return val;

	val &= ~mask;
	val |= set & mask;

	return __mt9t112_mcu_write(client, command, val);
}

static int mt9t112_reset(const struct i2c_client *client)
{
	int ret;

	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0001);
	usleep_range(1000, 5000);
	mt9t112_reg_mask_set(ret, client, 0x001a, 0x0001, 0x0000);

	return ret;
}

#ifndef EXT_CLOCK
#define CLOCK_INFO(a, b)
#else
#define CLOCK_INFO(a, b) mt9t112_clock_info(a, b)
static int mt9t112_clock_info(const struct i2c_client *client, u32 ext)
{
	int m, n, p1, p2, p3, p4, p5, p6, p7;
	u32 vco, clk;
	char *enable;

	ext /= 1000;  

	mt9t112_reg_read(n, client, 0x0012);
	p1 = n & 0x000f;
	n = n >> 4;
	p2 = n & 0x000f;
	n = n >> 4;
	p3 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002a);
	p4 = n & 0x000f;
	n = n >> 4;
	p5 = n & 0x000f;
	n = n >> 4;
	p6 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x002c);
	p7 = n & 0x000f;

	mt9t112_reg_read(n, client, 0x0010);
	m = n & 0x00ff;
	n = (n >> 8) & 0x003f;

	enable = ((ext < 6000) || (ext > 54000)) ? "X" : "";
	dev_dbg(&client->dev, "EXTCLK          : %10u K %s\n", ext, enable);

	vco = 2 * m * ext / (n + 1);
	enable = ((vco < 384000) || (vco > 768000)) ? "X" : "";
	dev_dbg(&client->dev, "VCO             : %10u K %s\n", vco, enable);

	clk = vco / (p1 + 1) / (p2 + 1);
	enable = (clk > 96000) ? "X" : "";
	dev_dbg(&client->dev, "PIXCLK          : %10u K %s\n", clk, enable);

	clk = vco / (p3 + 1);
	enable = (clk > 768000) ? "X" : "";
	dev_dbg(&client->dev, "MIPICLK         : %10u K %s\n", clk, enable);

	clk = vco / (p6 + 1);
	enable = (clk > 96000) ? "X" : "";
	dev_dbg(&client->dev, "MCU CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p5 + 1);
	enable = (clk > 54000) ? "X" : "";
	dev_dbg(&client->dev, "SOC CLK         : %10u K %s\n", clk, enable);

	clk = vco / (p4 + 1);
	enable = (clk > 70000) ? "X" : "";
	dev_dbg(&client->dev, "Sensor CLK      : %10u K %s\n", clk, enable);

	clk = vco / (p7 + 1);
	dev_dbg(&client->dev, "External sensor : %10u K\n", clk);

	clk = ext / (n + 1);
	enable = ((clk < 2000) || (clk > 24000)) ? "X" : "";
	dev_dbg(&client->dev, "PFD             : %10u K %s\n", clk, enable);

	return 0;
}
#endif

static int mt9t112_set_a_frame_size(const struct i2c_client *client,
				    u16 width, u16 height)
{
	int ret;
	u16 wstart = (MAX_WIDTH - width) / 2;
	u16 hstart = (MAX_HEIGHT - height) / 2;

	 
	mt9t112_mcu_write(ret, client, VAR(26, 0), width);
	mt9t112_mcu_write(ret, client, VAR(26, 2), height);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 43), 8 + width);
	mt9t112_mcu_write(ret, client, VAR(18, 45), 8 + height);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 2), 4 + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 4), 4 + wstart);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 6), 11 + height + hstart);
	mt9t112_mcu_write(ret, client, VAR(18, 8), 11 + width  + wstart);

	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	return ret;
}

static int mt9t112_set_pll_dividers(const struct i2c_client *client,
				    u8 m, u8 n, u8 p1, u8 p2, u8 p3, u8 p4,
				    u8 p5, u8 p6, u8 p7)
{
	int ret;
	u16 val;

	 
	val = (n << 8) | (m << 0);
	mt9t112_reg_mask_set(ret, client, 0x0010, 0x3fff, val);

	 
	val = ((p3 & 0x0F) << 8) | ((p2 & 0x0F) << 4) | ((p1 & 0x0F) << 0);
	mt9t112_reg_mask_set(ret, client, 0x0012, 0x0fff, val);

	 
	val = (0x7 << 12) | ((p6 & 0x0F) <<  8) | ((p5 & 0x0F) <<  4) |
	      ((p4 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002A, 0x7fff, val);

	 
	val = (0x1 << 12) | ((p7 & 0x0F) <<  0);
	mt9t112_reg_mask_set(ret, client, 0x002C, 0x100f, val);

	return ret;
}

static int mt9t112_init_pll(const struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	int data, i, ret;

	mt9t112_reg_mask_set(ret, client, 0x0014, 0x003, 0x0001);

	 
	mt9t112_reg_write(ret, client, 0x0014, 0x2145);

	 
	mt9t112_set_pll_dividers(client,
				 priv->info->divider.m, priv->info->divider.n,
				 priv->info->divider.p1, priv->info->divider.p2,
				 priv->info->divider.p3, priv->info->divider.p4,
				 priv->info->divider.p5, priv->info->divider.p6,
				 priv->info->divider.p7);

	 
	mt9t112_reg_write(ret, client, 0x0014, 0x2525);
	mt9t112_reg_write(ret, client, 0x0014, 0x2527);
	mt9t112_reg_write(ret, client, 0x0014, 0x3427);
	mt9t112_reg_write(ret, client, 0x0014, 0x3027);

	mdelay(10);

	 
	mt9t112_reg_write(ret, client, 0x0014, 0x3046);
	 
	mt9t112_reg_write(ret, client, 0x0016, 0x0400);
	mt9t112_reg_write(ret, client, 0x0022, 0x0190);
	mt9t112_reg_write(ret, client, 0x3B84, 0x0212);

	 
	mt9t112_reg_write(ret, client, 0x002E, 0x0500);

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0002, 0x0002);
	mt9t112_reg_mask_set(ret, client, 0x3B82, 0x0004, 0x0004);

	 
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0x0004);

	 
	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0001, 0);

	mdelay(50);

	 
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);
	mt9t112_reg_write(ret, client, 0x0614, 0x0001);
	mdelay(1);

	 
	for (i = 0; i < 100; i++) {
		mt9t112_reg_read(data, client, 0x0018);
		if (!(data & 0x4000))
			break;

		mdelay(10);
	}

	return ret;
}

static int mt9t112_init_setting(const struct i2c_client *client)
{
	int ret;

	 
	mt9t112_mcu_mask_set(ret, client, VAR(26, 160), 0x0040, 0x0000);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 12), 0x0024);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 15), 0x00CC);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 17), 0x01f1);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 19), 0x00fF);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 29), 0x032D);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 31), 0x073a);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 37), 0x07d0);

	 
	mt9t112_mcu_mask_set(ret, client, VAR(27, 160), 0x0040, 0x0000);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 74), 0x004);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 76), 0x004);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 78), 0x60B);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 80), 0x80B);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 87), 0x008C);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 89), 0x01F1);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 91), 0x00FF);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 101), 0x0668);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 103), 0x0AF0);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 109), 0x0AF0);

	 

	 
	mt9t112_mcu_write(ret, client, VAR8(8, 5), 0x01);

	 
	mt9t112_mcu_write(ret, client, VAR(27, 17), 0x0003);

	 
	mt9t112_mcu_write(ret, client, VAR(26, 17), 0x0003);

	 

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 165), 0x25);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 166), 0x28);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 167), 0x2C);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 168), 0x2F);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 68), 0xBA);

	 
	 
	mt9t112_mcu_write(ret, client, VAR8(18, 303), 0x00);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 69), 0x9B);

	 
	 
	mt9t112_mcu_write(ret, client, VAR8(18, 301), 0x00);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 140), 0x82);

	 
	 
	mt9t112_mcu_write(ret, client, VAR8(18, 304), 0x00);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 141), 0x6D);

	 
	 
	mt9t112_mcu_write(ret, client, VAR8(18, 302), 0x00);

	 
	mt9t112_mcu_write(ret, client, VAR8(8, 2), 0x10);

	 
	mt9t112_mcu_write(ret, client, VAR8(8, 9), 0x02);

	 
	mt9t112_mcu_write(ret, client, VAR8(8, 10), 0x03);

	 
	mt9t112_mcu_write(ret, client, VAR8(8, 12), 0x0A);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 70), 0x0014);

	 
	mt9t112_mcu_write(ret, client, VAR(18, 142), 0x0014);

	 
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0044), 133);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x0045), 110);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008c), 130);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x008d), 108);

	mt9t112_mcu_write(ret, client, VAR8(18, 0x00A5), 27);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a6), 30);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a7), 32);
	mt9t112_mcu_write(ret, client, VAR8(18, 0x00a8), 35);

	return ret;
}

static int mt9t112_auto_focus_setting(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR(12, 13),	0x000F);
	mt9t112_mcu_write(ret, client, VAR(12, 23),	0x0F0F);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x06);

	mt9t112_reg_write(ret, client, 0x0614, 0x0000);

	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);
	mt9t112_mcu_write(ret, client, VAR8(12, 2),	0x02);
	mt9t112_mcu_write(ret, client, VAR(12, 3),	0x0002);
	mt9t112_mcu_write(ret, client, VAR(17, 3),	0x8001);
	mt9t112_mcu_write(ret, client, VAR(17, 11),	0x0025);
	mt9t112_mcu_write(ret, client, VAR(17, 13),	0x0193);
	mt9t112_mcu_write(ret, client, VAR8(17, 33),	0x18);
	mt9t112_mcu_write(ret, client, VAR8(1, 0),	0x05);

	return ret;
}

static int mt9t112_auto_focus_trigger(const struct i2c_client *client)
{
	int ret;

	mt9t112_mcu_write(ret, client, VAR8(12, 25), 0x01);

	return ret;
}

static int mt9t112_init_camera(const struct i2c_client *client)
{
	int ret;

	ECHECKER(ret, mt9t112_reset(client));
	ECHECKER(ret, mt9t112_init_pll(client));
	ECHECKER(ret, mt9t112_init_setting(client));
	ECHECKER(ret, mt9t112_auto_focus_setting(client));

	mt9t112_reg_mask_set(ret, client, 0x0018, 0x0004, 0);

	 
	mt9t112_reg_write(ret, client, 0x3084, 0x2409);
	mt9t112_reg_write(ret, client, 0x3092, 0x0A49);
	mt9t112_reg_write(ret, client, 0x3094, 0x4949);
	mt9t112_reg_write(ret, client, 0x3096, 0x4950);

	 
	mt9t112_mcu_write(ret, client, VAR(26, 160), 0x0A2E);
	mt9t112_mcu_write(ret, client, VAR(27, 160), 0x0A2E);

	 
	mt9t112_mcu_write(ret, client, VAR(27, 144), 0x0CB4);

	 
	mt9t112_mcu_write(ret, client, VAR8(27, 142), 0x01);

	 
	mt9t112_reg_write(ret, client, 0x316C, 0x350F);

	 
	mt9t112_reg_write(ret, client, 0x1E, 0x777);

	return ret;
}

 

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int mt9t112_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int                ret;

	reg->size = 2;
	mt9t112_reg_read(ret, client, reg->reg);

	reg->val = (__u64)ret;

	return 0;
}

static int mt9t112_s_register(struct v4l2_subdev *sd,
			      const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	mt9t112_reg_write(ret, client, reg->reg, reg->val);

	return ret;
}
#endif

static int mt9t112_power_on(struct mt9t112_priv *priv)
{
	int ret;

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	if (priv->standby_gpio) {
		gpiod_set_value(priv->standby_gpio, 0);
		msleep(100);
	}

	return 0;
}

static int mt9t112_power_off(struct mt9t112_priv *priv)
{
	clk_disable_unprepare(priv->clk);
	if (priv->standby_gpio) {
		gpiod_set_value(priv->standby_gpio, 1);
		msleep(100);
	}

	return 0;
}

static int mt9t112_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	return on ? mt9t112_power_on(priv) :
		    mt9t112_power_off(priv);
}

static const struct v4l2_subdev_core_ops mt9t112_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= mt9t112_g_register,
	.s_register	= mt9t112_s_register,
#endif
	.s_power	= mt9t112_s_power,
};

 
static int mt9t112_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	int ret = 0;

	if (!enable) {
		 
		mt9t112_set_a_frame_size(client, VGA_WIDTH, VGA_HEIGHT);
		return ret;
	}

	if (!priv->init_done) {
		u16 param = MT9T112_FLAG_PCLK_RISING_EDGE & priv->info->flags ?
			    0x0001 : 0x0000;

		ECHECKER(ret, mt9t112_init_camera(client));

		 
		mt9t112_reg_write(ret, client, 0x3C20, param);

		mdelay(5);

		priv->init_done = true;
	}

	mt9t112_mcu_write(ret, client, VAR(26, 7), priv->format->fmt);
	mt9t112_mcu_write(ret, client, VAR(26, 9), priv->format->order);
	mt9t112_mcu_write(ret, client, VAR8(1, 0), 0x06);

	mt9t112_set_a_frame_size(client, priv->frame.width, priv->frame.height);

	ECHECKER(ret, mt9t112_auto_focus_trigger(client));

	dev_dbg(&client->dev, "format : %d\n", priv->format->code);
	dev_dbg(&client->dev, "size   : %d x %d\n",
		priv->frame.width,
		priv->frame.height);

	CLOCK_INFO(client, EXT_CLOCK);

	return ret;
}

static int mt9t112_set_params(struct mt9t112_priv *priv,
			      const struct v4l2_rect *rect,
			      u32 code)
{
	int i;

	 
	for (i = 0; i < priv->num_formats; i++)
		if (mt9t112_cfmts[i].code == code)
			break;

	if (i == priv->num_formats)
		return -EINVAL;

	priv->frame = *rect;

	 
	v4l_bound_align_image(&priv->frame.width, 0, MAX_WIDTH, 0,
			      &priv->frame.height, 0, MAX_HEIGHT, 0, 0);

	priv->format = mt9t112_cfmts + i;

	return 0;
}

static int mt9t112_get_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = MAX_WIDTH;
		sel->r.height = MAX_HEIGHT;
		return 0;
	case V4L2_SEL_TGT_CROP:
		sel->r = priv->frame;
		return 0;
	default:
		return -EINVAL;
	}
}

static int mt9t112_set_selection(struct v4l2_subdev *sd,
				 struct v4l2_subdev_state *sd_state,
				 struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	const struct v4l2_rect *rect = &sel->r;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE ||
	    sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	return mt9t112_set_params(priv, rect, priv->format->code);
}

static int mt9t112_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	if (format->pad)
		return -EINVAL;

	mf->width	= priv->frame.width;
	mf->height	= priv->frame.height;
	mf->colorspace	= priv->format->colorspace;
	mf->code	= priv->format->code;
	mf->field	= V4L2_FIELD_NONE;

	return 0;
}

static int mt9t112_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);
	struct v4l2_rect rect = {
		.width = mf->width,
		.height = mf->height,
		.left = priv->frame.left,
		.top = priv->frame.top,
	};
	int ret;

	ret = mt9t112_set_params(priv, &rect, mf->code);

	if (!ret)
		mf->colorspace = priv->format->colorspace;

	return ret;
}

static int mt9t112_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_state *sd_state,
			   struct v4l2_subdev_format *format)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct mt9t112_priv *priv = to_mt9t112(client);
	int i;

	if (format->pad)
		return -EINVAL;

	for (i = 0; i < priv->num_formats; i++)
		if (mt9t112_cfmts[i].code == mf->code)
			break;

	if (i == priv->num_formats) {
		mf->code = MEDIA_BUS_FMT_UYVY8_2X8;
		mf->colorspace = V4L2_COLORSPACE_JPEG;
	} else {
		mf->colorspace = mt9t112_cfmts[i].colorspace;
	}

	v4l_bound_align_image(&mf->width, 0, MAX_WIDTH, 0,
			      &mf->height, 0, MAX_HEIGHT, 0, 0);

	mf->field = V4L2_FIELD_NONE;

	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		return mt9t112_s_fmt(sd, mf);
	sd_state->pads->try_fmt = *mf;

	return 0;
}

static int mt9t112_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_state *sd_state,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct mt9t112_priv *priv = to_mt9t112(client);

	if (code->pad || code->index >= priv->num_formats)
		return -EINVAL;

	code->code = mt9t112_cfmts[code->index].code;

	return 0;
}

static const struct v4l2_subdev_video_ops mt9t112_subdev_video_ops = {
	.s_stream	= mt9t112_s_stream,
};

static const struct v4l2_subdev_pad_ops mt9t112_subdev_pad_ops = {
	.enum_mbus_code	= mt9t112_enum_mbus_code,
	.get_selection	= mt9t112_get_selection,
	.set_selection	= mt9t112_set_selection,
	.get_fmt	= mt9t112_get_fmt,
	.set_fmt	= mt9t112_set_fmt,
};

 
static const struct v4l2_subdev_ops mt9t112_subdev_ops = {
	.core	= &mt9t112_subdev_core_ops,
	.video	= &mt9t112_subdev_video_ops,
	.pad	= &mt9t112_subdev_pad_ops,
};

static int mt9t112_camera_probe(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);
	const char          *devname;
	int                  chipid;
	int		     ret;

	ret = mt9t112_s_power(&priv->subdev, 1);
	if (ret < 0)
		return ret;

	 
	mt9t112_reg_read(chipid, client, 0x0000);

	switch (chipid) {
	case 0x2680:
		devname = "mt9t111";
		priv->num_formats = 1;
		break;
	case 0x2682:
		devname = "mt9t112";
		priv->num_formats = ARRAY_SIZE(mt9t112_cfmts);
		break;
	default:
		dev_err(&client->dev, "Product ID error %04x\n", chipid);
		ret = -ENODEV;
		goto done;
	}

	dev_info(&client->dev, "%s chip ID %04x\n", devname, chipid);

done:
	mt9t112_s_power(&priv->subdev, 0);

	return ret;
}

static int mt9t112_probe(struct i2c_client *client)
{
	struct mt9t112_priv *priv;
	int ret;

	if (!client->dev.platform_data) {
		dev_err(&client->dev, "mt9t112: missing platform data!\n");
		return -EINVAL;
	}

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->info = client->dev.platform_data;
	priv->init_done = false;

	v4l2_i2c_subdev_init(&priv->subdev, client, &mt9t112_subdev_ops);

	priv->clk = devm_clk_get(&client->dev, "extclk");
	if (PTR_ERR(priv->clk) == -ENOENT) {
		priv->clk = NULL;
	} else if (IS_ERR(priv->clk)) {
		dev_err(&client->dev, "Unable to get clock \"extclk\"\n");
		return PTR_ERR(priv->clk);
	}

	priv->standby_gpio = devm_gpiod_get_optional(&client->dev, "standby",
						     GPIOD_OUT_HIGH);
	if (IS_ERR(priv->standby_gpio)) {
		dev_err(&client->dev, "Unable to get gpio \"standby\"\n");
		return PTR_ERR(priv->standby_gpio);
	}

	ret = mt9t112_camera_probe(client);
	if (ret)
		return ret;

	return v4l2_async_register_subdev(&priv->subdev);
}

static void mt9t112_remove(struct i2c_client *client)
{
	struct mt9t112_priv *priv = to_mt9t112(client);

	clk_disable_unprepare(priv->clk);
	v4l2_async_unregister_subdev(&priv->subdev);
}

static const struct i2c_device_id mt9t112_id[] = {
	{ "mt9t112", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mt9t112_id);

static struct i2c_driver mt9t112_i2c_driver = {
	.driver = {
		.name = "mt9t112",
	},
	.probe    = mt9t112_probe,
	.remove   = mt9t112_remove,
	.id_table = mt9t112_id,
};

module_i2c_driver(mt9t112_i2c_driver);

MODULE_DESCRIPTION("V4L2 driver for MT9T111/MT9T112 camera sensor");
MODULE_AUTHOR("Kuninori Morimoto");
MODULE_LICENSE("GPL v2");
