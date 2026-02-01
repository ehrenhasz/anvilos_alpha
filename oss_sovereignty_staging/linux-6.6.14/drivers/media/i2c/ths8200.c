 

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/v4l2-dv-timings.h>

#include <media/v4l2-dv-timings.h>
#include <media/v4l2-async.h>
#include <media/v4l2-device.h>

#include "ths8200_regs.h"

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "debug level (0-2)");

MODULE_DESCRIPTION("Texas Instruments THS8200 video encoder driver");
MODULE_AUTHOR("Mats Randgaard <mats.randgaard@cisco.com>");
MODULE_AUTHOR("Martin Bugge <martin.bugge@cisco.com>");
MODULE_LICENSE("GPL v2");

struct ths8200_state {
	struct v4l2_subdev sd;
	uint8_t chip_version;
	 
	bool power_on;
	struct v4l2_dv_timings dv_timings;
};

static const struct v4l2_dv_timings_cap ths8200_timings_cap = {
	.type = V4L2_DV_BT_656_1120,
	 
	.reserved = { 0 },
	V4L2_INIT_BT_TIMINGS(640, 1920, 350, 1080, 25000000, 148500000,
		V4L2_DV_BT_STD_CEA861, V4L2_DV_BT_CAP_PROGRESSIVE)
};

static inline struct ths8200_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct ths8200_state, sd);
}

static inline unsigned htotal(const struct v4l2_bt_timings *t)
{
	return V4L2_DV_BT_FRAME_WIDTH(t);
}

static inline unsigned vtotal(const struct v4l2_bt_timings *t)
{
	return V4L2_DV_BT_FRAME_HEIGHT(t);
}

static int ths8200_read(struct v4l2_subdev *sd, u8 reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_read_byte_data(client, reg);
}

static int ths8200_write(struct v4l2_subdev *sd, u8 reg, u8 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int i;

	for (i = 0; i < 3; i++) {
		ret = i2c_smbus_write_byte_data(client, reg, val);
		if (ret == 0)
			return 0;
	}
	v4l2_err(sd, "I2C Write Problem\n");
	return ret;
}

 
static inline void
ths8200_write_and_or(struct v4l2_subdev *sd, u8 reg,
		     uint8_t clr_mask, uint8_t val_mask)
{
	ths8200_write(sd, reg, (ths8200_read(sd, reg) & clr_mask) | val_mask);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG

static int ths8200_g_register(struct v4l2_subdev *sd,
			      struct v4l2_dbg_register *reg)
{
	reg->val = ths8200_read(sd, reg->reg & 0xff);
	reg->size = 1;

	return 0;
}

static int ths8200_s_register(struct v4l2_subdev *sd,
			      const struct v4l2_dbg_register *reg)
{
	ths8200_write(sd, reg->reg & 0xff, reg->val & 0xff);

	return 0;
}
#endif

static int ths8200_log_status(struct v4l2_subdev *sd)
{
	struct ths8200_state *state = to_state(sd);
	uint8_t reg_03 = ths8200_read(sd, THS8200_CHIP_CTL);

	v4l2_info(sd, "----- Chip status -----\n");
	v4l2_info(sd, "version: %u\n", state->chip_version);
	v4l2_info(sd, "power: %s\n", (reg_03 & 0x0c) ? "off" : "on");
	v4l2_info(sd, "reset: %s\n", (reg_03 & 0x01) ? "off" : "on");
	v4l2_info(sd, "test pattern: %s\n",
		  (reg_03 & 0x20) ? "enabled" : "disabled");
	v4l2_info(sd, "format: %ux%u\n",
		  ths8200_read(sd, THS8200_DTG2_PIXEL_CNT_MSB) * 256 +
		  ths8200_read(sd, THS8200_DTG2_PIXEL_CNT_LSB),
		  (ths8200_read(sd, THS8200_DTG2_LINE_CNT_MSB) & 0x07) * 256 +
		  ths8200_read(sd, THS8200_DTG2_LINE_CNT_LSB));
	v4l2_print_dv_timings(sd->name, "Configured format:",
			      &state->dv_timings, true);
	return 0;
}

 
static int ths8200_s_power(struct v4l2_subdev *sd, int on)
{
	struct ths8200_state *state = to_state(sd);

	v4l2_dbg(1, debug, sd, "%s: power %s\n", __func__, on ? "on" : "off");

	state->power_on = on;

	 
	ths8200_write_and_or(sd, THS8200_CHIP_CTL, 0xf2, (on ? 0x00 : 0x0c));

	return 0;
}

static const struct v4l2_subdev_core_ops ths8200_core_ops = {
	.log_status = ths8200_log_status,
	.s_power = ths8200_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = ths8200_g_register,
	.s_register = ths8200_s_register,
#endif
};

 

static int ths8200_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct ths8200_state *state = to_state(sd);

	if (enable && !state->power_on)
		ths8200_s_power(sd, true);

	ths8200_write_and_or(sd, THS8200_CHIP_CTL, 0xfe,
			     (enable ? 0x01 : 0x00));

	v4l2_dbg(1, debug, sd, "%s: %sable\n",
		 __func__, (enable ? "en" : "dis"));

	return 0;
}

static void ths8200_core_init(struct v4l2_subdev *sd)
{
	 
	ths8200_write_and_or(sd, THS8200_CHIP_CTL, 0x3f, 0xc0);

	 
	 
	ths8200_write(sd, THS8200_DATA_CNTL, 0x70);

	 
	ths8200_write(sd, THS8200_DTG1_MODE, 0x87);

	 

	 
	ths8200_write(sd, THS8200_DTG1_Y_SYNC_MSB, 0x00);
	ths8200_write(sd, THS8200_DTG1_CBCR_SYNC_MSB, 0x00);
}

static void ths8200_setup(struct v4l2_subdev *sd, struct v4l2_bt_timings *bt)
{
	uint8_t polarity = 0;
	uint16_t line_start_active_video = (bt->vsync + bt->vbackporch);
	uint16_t line_start_front_porch  = (vtotal(bt) - bt->vfrontporch);

	 
	 
	ths8200_s_stream(sd, false);

	 
	ths8200_write(sd, THS8200_DTG1_SPEC_A, bt->hsync);
	ths8200_write(sd, THS8200_DTG1_SPEC_B, bt->hfrontporch);

	 
	if (!bt->interlaced)
		ths8200_write(sd, THS8200_DTG1_SPEC_C, 0x00);

	 
	ths8200_write(sd, THS8200_DTG1_SPEC_D_LSB,
		      (bt->hbackporch + bt->hsync) & 0xff);
	 
	ths8200_write(sd, THS8200_DTG1_SPEC_E_LSB, 0x00);
	 
	ths8200_write(sd, THS8200_DTG1_SPEC_DEH_MSB,
		      ((bt->hbackporch + bt->hsync) & 0x100) >> 1);

	 
	ths8200_write(sd, THS8200_DTG1_SPEC_K_LSB, (bt->hfrontporch) & 0xff);
	ths8200_write(sd, THS8200_DTG1_SPEC_K_MSB,
		      ((bt->hfrontporch) & 0x700) >> 8);

	 
	ths8200_write(sd, THS8200_DTG1_SPEC_G_LSB, (htotal(bt)/2) & 0xff);
	ths8200_write(sd, THS8200_DTG1_SPEC_G_MSB,
		      ((htotal(bt)/2) >> 8) & 0x0f);

	 
	ths8200_write(sd, THS8200_DTG1_TOT_PIXELS_MSB, htotal(bt) >> 8);
	ths8200_write(sd, THS8200_DTG1_TOT_PIXELS_LSB, htotal(bt) & 0xff);

	 
	 
	ths8200_write(sd, THS8200_DTG1_FRAME_FIELD_SZ_MSB,
		      ((vtotal(bt) >> 4) & 0xf0) + 0x7);
	ths8200_write(sd, THS8200_DTG1_FRAME_SZ_LSB, vtotal(bt) & 0xff);

	 
	if (!bt->interlaced)
		ths8200_write(sd, THS8200_DTG1_FIELD_SZ_LSB, 0xff);

	 
	 
	ths8200_write_and_or(sd, THS8200_DTG2_BP1_2_MSB, 0x88,
			     ((line_start_active_video >> 4) & 0x70) +
			     ((line_start_front_porch >> 8) & 0x07));
	ths8200_write(sd, THS8200_DTG2_BP3_4_MSB, ((vtotal(bt)) >> 4) & 0x70);
	ths8200_write(sd, THS8200_DTG2_BP1_LSB, line_start_active_video & 0xff);
	ths8200_write(sd, THS8200_DTG2_BP2_LSB, line_start_front_porch & 0xff);
	ths8200_write(sd, THS8200_DTG2_BP3_LSB, (vtotal(bt)) & 0xff);

	 
	ths8200_write(sd, THS8200_DTG2_LINETYPE1, 0x90);
	ths8200_write(sd, THS8200_DTG2_LINETYPE2, 0x90);

	 
	ths8200_write(sd, THS8200_DTG2_HLENGTH_LSB, bt->hsync & 0xff);
	ths8200_write_and_or(sd, THS8200_DTG2_HLENGTH_LSB_HDLY_MSB, 0x3f,
			     (bt->hsync >> 2) & 0xc0);

	 
	ths8200_write_and_or(sd, THS8200_DTG2_HLENGTH_LSB_HDLY_MSB, 0xe0,
			     (htotal(bt) >> 8) & 0x1f);
	ths8200_write(sd, THS8200_DTG2_HLENGTH_HDLY_LSB, htotal(bt));

	 
	ths8200_write(sd, THS8200_DTG2_VLENGTH1_LSB, (bt->vsync + 1) & 0xff);
	ths8200_write_and_or(sd, THS8200_DTG2_VLENGTH1_MSB_VDLY1_MSB, 0x3f,
			     ((bt->vsync + 1) >> 2) & 0xc0);

	 
	ths8200_write_and_or(sd, THS8200_DTG2_VLENGTH1_MSB_VDLY1_MSB, 0xf8,
			     ((vtotal(bt) + 1) >> 8) & 0x7);
	ths8200_write(sd, THS8200_DTG2_VDLY1_LSB, vtotal(bt) + 1);

	 
	ths8200_write(sd, THS8200_DTG2_VLENGTH2_LSB, 0x00);
	ths8200_write(sd, THS8200_DTG2_VLENGTH2_MSB_VDLY2_MSB, 0x07);
	ths8200_write(sd, THS8200_DTG2_VDLY2_LSB, 0xff);

	 
	 
	ths8200_write(sd, THS8200_DTG2_HS_IN_DLY_MSB, 0);
	ths8200_write(sd, THS8200_DTG2_HS_IN_DLY_LSB, 0);
	ths8200_write(sd, THS8200_DTG2_VS_IN_DLY_MSB, 0);
	ths8200_write(sd, THS8200_DTG2_VS_IN_DLY_LSB, 0);

	 
	if (bt->polarities & V4L2_DV_HSYNC_POS_POL) {
		polarity |= 0x01;  
		polarity |= 0x08;  
	}
	if (bt->polarities & V4L2_DV_VSYNC_POS_POL) {
		polarity |= 0x02;  
		polarity |= 0x10;  
	}

	 
	 
	ths8200_write(sd, THS8200_DTG2_CNTL, 0x44 | polarity);

	 
	ths8200_s_stream(sd, true);

	v4l2_dbg(1, debug, sd, "%s: frame %dx%d, polarity %d\n"
		 "horizontal: front porch %d, back porch %d, sync %d\n"
		 "vertical: sync %d\n", __func__, htotal(bt), vtotal(bt),
		 polarity, bt->hfrontporch, bt->hbackporch,
		 bt->hsync, bt->vsync);
}

static int ths8200_s_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings)
{
	struct ths8200_state *state = to_state(sd);

	v4l2_dbg(1, debug, sd, "%s:\n", __func__);

	if (!v4l2_valid_dv_timings(timings, &ths8200_timings_cap,
				NULL, NULL))
		return -EINVAL;

	if (!v4l2_find_dv_timings_cap(timings, &ths8200_timings_cap, 10,
				NULL, NULL)) {
		v4l2_dbg(1, debug, sd, "Unsupported format\n");
		return -EINVAL;
	}

	timings->bt.flags &= ~V4L2_DV_FL_REDUCED_FPS;

	 
	state->dv_timings = *timings;

	ths8200_setup(sd, &timings->bt);

	return 0;
}

static int ths8200_g_dv_timings(struct v4l2_subdev *sd,
				struct v4l2_dv_timings *timings)
{
	struct ths8200_state *state = to_state(sd);

	v4l2_dbg(1, debug, sd, "%s:\n", __func__);

	*timings = state->dv_timings;

	return 0;
}

static int ths8200_enum_dv_timings(struct v4l2_subdev *sd,
				   struct v4l2_enum_dv_timings *timings)
{
	if (timings->pad != 0)
		return -EINVAL;

	return v4l2_enum_dv_timings_cap(timings, &ths8200_timings_cap,
			NULL, NULL);
}

static int ths8200_dv_timings_cap(struct v4l2_subdev *sd,
				  struct v4l2_dv_timings_cap *cap)
{
	if (cap->pad != 0)
		return -EINVAL;

	*cap = ths8200_timings_cap;
	return 0;
}

 
static const struct v4l2_subdev_video_ops ths8200_video_ops = {
	.s_stream = ths8200_s_stream,
	.s_dv_timings = ths8200_s_dv_timings,
	.g_dv_timings = ths8200_g_dv_timings,
};

static const struct v4l2_subdev_pad_ops ths8200_pad_ops = {
	.enum_dv_timings = ths8200_enum_dv_timings,
	.dv_timings_cap = ths8200_dv_timings_cap,
};

 
static const struct v4l2_subdev_ops ths8200_ops = {
	.core  = &ths8200_core_ops,
	.video = &ths8200_video_ops,
	.pad = &ths8200_pad_ops,
};

static int ths8200_probe(struct i2c_client *client)
{
	struct ths8200_state *state;
	struct v4l2_subdev *sd;
	int error;

	 
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	state = devm_kzalloc(&client->dev, sizeof(*state), GFP_KERNEL);
	if (!state)
		return -ENOMEM;

	sd = &state->sd;
	v4l2_i2c_subdev_init(sd, client, &ths8200_ops);

	state->chip_version = ths8200_read(sd, THS8200_VERSION);
	v4l2_dbg(1, debug, sd, "chip version 0x%x\n", state->chip_version);

	ths8200_core_init(sd);

	error = v4l2_async_register_subdev(&state->sd);
	if (error)
		return error;

	v4l2_info(sd, "%s found @ 0x%x (%s)\n", client->name,
		  client->addr << 1, client->adapter->name);

	return 0;
}

static void ths8200_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ths8200_state *decoder = to_state(sd);

	v4l2_dbg(1, debug, sd, "%s removed @ 0x%x (%s)\n", client->name,
		 client->addr << 1, client->adapter->name);

	ths8200_s_power(sd, false);
	v4l2_async_unregister_subdev(&decoder->sd);
}

static const struct i2c_device_id ths8200_id[] = {
	{ "ths8200", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, ths8200_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ths8200_of_match[] = {
	{ .compatible = "ti,ths8200", },
	{   },
};
MODULE_DEVICE_TABLE(of, ths8200_of_match);
#endif

static struct i2c_driver ths8200_driver = {
	.driver = {
		.name = "ths8200",
		.of_match_table = of_match_ptr(ths8200_of_match),
	},
	.probe = ths8200_probe,
	.remove = ths8200_remove,
	.id_table = ths8200_id,
};

module_i2c_driver(ths8200_driver);
