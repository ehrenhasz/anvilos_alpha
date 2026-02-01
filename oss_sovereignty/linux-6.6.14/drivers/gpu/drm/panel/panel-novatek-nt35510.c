
 
#include <linux/backlight.h>
#include <linux/bitops.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#define MCS_CMD_MAUCCTR		0xF0  
#define MCS_CMD_READ_ID1	0xDA
#define MCS_CMD_READ_ID2	0xDB
#define MCS_CMD_READ_ID3	0xDC
#define MCS_CMD_MTP_READ_SETTING 0xF8  
#define MCS_CMD_MTP_READ_PARAM 0xFF  

 
#define NT35510_P0_DOPCTR 0xB1
#define NT35510_P0_SDHDTCTR 0xB6
#define NT35510_P0_GSEQCTR 0xB7
#define NT35510_P0_SDEQCTR 0xB8
#define NT35510_P0_SDVPCTR 0xBA
#define NT35510_P0_DPFRCTR1 0xBD
#define NT35510_P0_DPFRCTR2 0xBE
#define NT35510_P0_DPFRCTR3 0xBF
#define NT35510_P0_DPMCTR12 0xCC

#define NT35510_P0_DOPCTR_LEN 2
#define NT35510_P0_GSEQCTR_LEN 2
#define NT35510_P0_SDEQCTR_LEN 4
#define NT35510_P0_SDVPCTR_LEN 1
#define NT35510_P0_DPFRCTR1_LEN 5
#define NT35510_P0_DPFRCTR2_LEN 5
#define NT35510_P0_DPFRCTR3_LEN 5
#define NT35510_P0_DPMCTR12_LEN 3

#define NT35510_DOPCTR_0_RAMKP BIT(7)  
#define NT35510_DOPCTR_0_DSITE BIT(6)  
#define NT35510_DOPCTR_0_DSIG BIT(5)  
#define NT35510_DOPCTR_0_DSIM BIT(4)  
#define NT35510_DOPCTR_0_EOTP BIT(3)  
#define NT35510_DOPCTR_0_N565 BIT(2)  
#define NT35510_DOPCTR_1_TW_PWR_SEL BIT(4)  
#define NT35510_DOPCTR_1_CRGB BIT(3)  
#define NT35510_DOPCTR_1_CTB BIT(2)  
#define NT35510_DOPCTR_1_CRL BIT(1)  
#define NT35510_P0_SDVPCTR_PRG BIT(2)  
#define NT35510_P0_SDVPCTR_AVDD 0  
#define NT35510_P0_SDVPCTR_OFFCOL 1  
#define NT35510_P0_SDVPCTR_AVSS 2  
#define NT35510_P0_SDVPCTR_HI_Z 3  

 
#define NT35510_P1_SETAVDD 0xB0
#define NT35510_P1_SETAVEE 0xB1
#define NT35510_P1_SETVCL 0xB2
#define NT35510_P1_SETVGH 0xB3
#define NT35510_P1_SETVRGH 0xB4
#define NT35510_P1_SETVGL 0xB5
#define NT35510_P1_BT1CTR 0xB6
#define NT35510_P1_BT2CTR 0xB7
#define NT35510_P1_BT3CTR 0xB8
#define NT35510_P1_BT4CTR 0xB9  
#define NT35510_P1_BT5CTR 0xBA
#define NT35510_P1_PFMCTR 0xBB
#define NT35510_P1_SETVGP 0xBC
#define NT35510_P1_SETVGN 0xBD
#define NT35510_P1_SETVCMOFF 0xBE
#define NT35510_P1_VGHCTR 0xBF  
#define NT35510_P1_SET_GAMMA_RED_POS 0xD1
#define NT35510_P1_SET_GAMMA_GREEN_POS 0xD2
#define NT35510_P1_SET_GAMMA_BLUE_POS 0xD3
#define NT35510_P1_SET_GAMMA_RED_NEG 0xD4
#define NT35510_P1_SET_GAMMA_GREEN_NEG 0xD5
#define NT35510_P1_SET_GAMMA_BLUE_NEG 0xD6

 
#define NT35510_P1_AVDD_LEN 3
#define NT35510_P1_AVEE_LEN 3
#define NT35510_P1_VGH_LEN 3
#define NT35510_P1_VGL_LEN 3
#define NT35510_P1_VGP_LEN 3
#define NT35510_P1_VGN_LEN 3
 
#define NT35510_P1_BT1CTR_LEN 3
#define NT35510_P1_BT2CTR_LEN 3
#define NT35510_P1_BT4CTR_LEN 3
#define NT35510_P1_BT5CTR_LEN 3
 
#define NT35510_P1_GAMMA_LEN 52

 
struct nt35510_config {
	 
	u32 width_mm;
	 
	u32 height_mm;
	 
	const struct drm_display_mode mode;
	 
	u8 avdd[NT35510_P1_AVDD_LEN];
	 
	u8 bt1ctr[NT35510_P1_BT1CTR_LEN];
	 
	u8 avee[NT35510_P1_AVEE_LEN];
	 
	u8 bt2ctr[NT35510_P1_BT2CTR_LEN];
	 
	u8 vgh[NT35510_P1_VGH_LEN];
	 
	u8 bt4ctr[NT35510_P1_BT4CTR_LEN];
	 
	u8 vgl[NT35510_P1_VGL_LEN];
	 
	u8 bt5ctr[NT35510_P1_BT5CTR_LEN];
	 
	u8 vgp[NT35510_P1_VGP_LEN];
	 
	u8 vgn[NT35510_P1_VGN_LEN];
	 
	u8 sdeqctr[NT35510_P0_SDEQCTR_LEN];
	 
	u8 sdvpctr;
	 
	u16 t1;
	 
	u8 vbp;
	 
	u8 vfp;
	 
	u8 psel;
	 
	u8 dpmctr12[NT35510_P0_DPMCTR12_LEN];
	 
	u8 gamma_corr_pos_r[NT35510_P1_GAMMA_LEN];
	 
	u8 gamma_corr_pos_g[NT35510_P1_GAMMA_LEN];
	 
	u8 gamma_corr_pos_b[NT35510_P1_GAMMA_LEN];
	 
	u8 gamma_corr_neg_r[NT35510_P1_GAMMA_LEN];
	 
	u8 gamma_corr_neg_g[NT35510_P1_GAMMA_LEN];
	 
	u8 gamma_corr_neg_b[NT35510_P1_GAMMA_LEN];
};

 
struct nt35510 {
	 
	struct device *dev;
	 
	const struct nt35510_config *conf;
	 
	struct drm_panel panel;
	 
	struct regulator_bulk_data supplies[2];
	 
	struct gpio_desc *reset_gpio;
};

 
static const u8 nt35510_mauc_mtp_read_param[] = { 0xAA, 0x55, 0x25, 0x01 };
static const u8 nt35510_mauc_mtp_read_setting[] = { 0x01, 0x02, 0x00, 0x20,
						    0x33, 0x13, 0x00, 0x40,
						    0x00, 0x00, 0x23, 0x02 };
static const u8 nt35510_mauc_select_page_0[] = { 0x55, 0xAA, 0x52, 0x08, 0x00 };
static const u8 nt35510_mauc_select_page_1[] = { 0x55, 0xAA, 0x52, 0x08, 0x01 };
static const u8 nt35510_vgh_on[] = { 0x01 };

static inline struct nt35510 *panel_to_nt35510(struct drm_panel *panel)
{
	return container_of(panel, struct nt35510, panel);
}

#define NT35510_ROTATE_0_SETTING	0x02
#define NT35510_ROTATE_180_SETTING	0x00

static int nt35510_send_long(struct nt35510 *nt, struct mipi_dsi_device *dsi,
			     u8 cmd, u8 cmdlen, const u8 *seq)
{
	const u8 *seqp = seq;
	int cmdwritten = 0;
	int chunk = cmdlen;
	int ret;

	if (chunk > 15)
		chunk = 15;
	ret = mipi_dsi_dcs_write(dsi, cmd, seqp, chunk);
	if (ret < 0) {
		dev_err(nt->dev, "error sending DCS command seq cmd %02x\n", cmd);
		return ret;
	}
	cmdwritten += chunk;
	seqp += chunk;

	while (cmdwritten < cmdlen) {
		chunk = cmdlen - cmdwritten;
		if (chunk > 15)
			chunk = 15;
		ret = mipi_dsi_generic_write(dsi, seqp, chunk);
		if (ret < 0) {
			dev_err(nt->dev, "error sending generic write seq %02x\n", cmd);
			return ret;
		}
		cmdwritten += chunk;
		seqp += chunk;
	}
	dev_dbg(nt->dev, "sent command %02x %02x bytes\n", cmd, cmdlen);
	return 0;
}

static int nt35510_read_id(struct nt35510 *nt)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	u8 id1, id2, id3;
	int ret;

	ret = mipi_dsi_dcs_read(dsi, MCS_CMD_READ_ID1, &id1, 1);
	if (ret < 0) {
		dev_err(nt->dev, "could not read MTP ID1\n");
		return ret;
	}
	ret = mipi_dsi_dcs_read(dsi, MCS_CMD_READ_ID2, &id2, 1);
	if (ret < 0) {
		dev_err(nt->dev, "could not read MTP ID2\n");
		return ret;
	}
	ret = mipi_dsi_dcs_read(dsi, MCS_CMD_READ_ID3, &id3, 1);
	if (ret < 0) {
		dev_err(nt->dev, "could not read MTP ID3\n");
		return ret;
	}

	 
	dev_info(nt->dev, "MTP ID manufacturer: %02x version: %02x driver: %02x\n", id1, id2, id3);

	return 0;
}

 
static int nt35510_setup_power(struct nt35510 *nt)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	int ret;

	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETAVDD,
				NT35510_P1_AVDD_LEN,
				nt->conf->avdd);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_BT1CTR,
				NT35510_P1_BT1CTR_LEN,
				nt->conf->bt1ctr);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETAVEE,
				NT35510_P1_AVEE_LEN,
				nt->conf->avee);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_BT2CTR,
				NT35510_P1_BT2CTR_LEN,
				nt->conf->bt2ctr);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETVGH,
				NT35510_P1_VGH_LEN,
				nt->conf->vgh);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_BT4CTR,
				NT35510_P1_BT4CTR_LEN,
				nt->conf->bt4ctr);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_VGHCTR,
				ARRAY_SIZE(nt35510_vgh_on),
				nt35510_vgh_on);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETVGL,
				NT35510_P1_VGL_LEN,
				nt->conf->vgl);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_BT5CTR,
				NT35510_P1_BT5CTR_LEN,
				nt->conf->bt5ctr);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETVGP,
				NT35510_P1_VGP_LEN,
				nt->conf->vgp);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SETVGN,
				NT35510_P1_VGN_LEN,
				nt->conf->vgn);
	if (ret)
		return ret;

	 
	usleep_range(10000, 20000);

	return 0;
}

 
static int nt35510_setup_display(struct nt35510 *nt)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	const struct nt35510_config *conf = nt->conf;
	u8 dopctr[NT35510_P0_DOPCTR_LEN];
	u8 gseqctr[NT35510_P0_GSEQCTR_LEN];
	u8 dpfrctr[NT35510_P0_DPFRCTR1_LEN];
	 
	u8 addr_mode = NT35510_ROTATE_0_SETTING;
	u8 val;
	int ret;

	 
	dopctr[0] = NT35510_DOPCTR_0_DSITE | NT35510_DOPCTR_0_EOTP |
		NT35510_DOPCTR_0_N565;
	dopctr[1] = NT35510_DOPCTR_1_CTB;
	ret = nt35510_send_long(nt, dsi, NT35510_P0_DOPCTR,
				NT35510_P0_DOPCTR_LEN,
				dopctr);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_ADDRESS_MODE, &addr_mode,
				 sizeof(addr_mode));
	if (ret < 0)
		return ret;

	 
	val = 0x0A;
	ret = mipi_dsi_dcs_write(dsi, NT35510_P0_SDHDTCTR, &val,
				 sizeof(val));
	if (ret < 0)
		return ret;

	 
	gseqctr[0] = 0x00;
	gseqctr[1] = 0x00;
	ret = nt35510_send_long(nt, dsi, NT35510_P0_GSEQCTR,
				NT35510_P0_GSEQCTR_LEN,
				gseqctr);
	if (ret)
		return ret;

	ret = nt35510_send_long(nt, dsi, NT35510_P0_SDEQCTR,
				NT35510_P0_SDEQCTR_LEN,
				conf->sdeqctr);
	if (ret)
		return ret;

	ret = mipi_dsi_dcs_write(dsi, NT35510_P0_SDVPCTR,
				 &conf->sdvpctr, 1);
	if (ret < 0)
		return ret;

	 
	dpfrctr[0] = (conf->t1 >> 8) & 0xFF;
	dpfrctr[1] = conf->t1 & 0xFF;
	 
	dpfrctr[2] = conf->vbp;
	 
	dpfrctr[3] = conf->vfp;
	dpfrctr[4] = conf->psel;
	ret = nt35510_send_long(nt, dsi, NT35510_P0_DPFRCTR1,
				NT35510_P0_DPFRCTR1_LEN,
				dpfrctr);
	if (ret)
		return ret;
	 
	dpfrctr[3]--;
	ret = nt35510_send_long(nt, dsi, NT35510_P0_DPFRCTR2,
				NT35510_P0_DPFRCTR2_LEN,
				dpfrctr);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P0_DPFRCTR3,
				NT35510_P0_DPFRCTR3_LEN,
				dpfrctr);
	if (ret)
		return ret;

	 
	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret)
		return ret;

	 
	ret = nt35510_send_long(nt, dsi, NT35510_P0_DPMCTR12,
				NT35510_P0_DPMCTR12_LEN,
				conf->dpmctr12);
	if (ret)
		return ret;

	return 0;
}

static int nt35510_set_brightness(struct backlight_device *bl)
{
	struct nt35510 *nt = bl_get_data(bl);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	u8 brightness = bl->props.brightness;
	int ret;

	dev_dbg(nt->dev, "set brightness %d\n", brightness);
	ret = mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				 &brightness,
				 sizeof(brightness));
	if (ret < 0)
		return ret;

	return 0;
}

static const struct backlight_ops nt35510_bl_ops = {
	.update_status = nt35510_set_brightness,
};

 
static int nt35510_power_on(struct nt35510 *nt)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(nt->supplies), nt->supplies);
	if (ret < 0) {
		dev_err(nt->dev, "unable to enable regulators\n");
		return ret;
	}

	 
	if (nt->reset_gpio) {
		gpiod_set_value(nt->reset_gpio, 1);
		 
		usleep_range(20, 1000);
		gpiod_set_value(nt->reset_gpio, 0);
		 
		usleep_range(120000, 140000);
	}

	ret = nt35510_send_long(nt, dsi, MCS_CMD_MTP_READ_PARAM,
				ARRAY_SIZE(nt35510_mauc_mtp_read_param),
				nt35510_mauc_mtp_read_param);
	if (ret)
		return ret;

	ret = nt35510_send_long(nt, dsi, MCS_CMD_MTP_READ_SETTING,
				ARRAY_SIZE(nt35510_mauc_mtp_read_setting),
				nt35510_mauc_mtp_read_setting);
	if (ret)
		return ret;

	nt35510_read_id(nt);

	 
	ret = nt35510_send_long(nt, dsi, MCS_CMD_MAUCCTR,
				ARRAY_SIZE(nt35510_mauc_select_page_1),
				nt35510_mauc_select_page_1);
	if (ret)
		return ret;

	ret = nt35510_setup_power(nt);
	if (ret)
		return ret;

	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_RED_POS,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_pos_r);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_GREEN_POS,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_pos_g);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_BLUE_POS,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_pos_b);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_RED_NEG,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_neg_r);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_GREEN_NEG,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_neg_g);
	if (ret)
		return ret;
	ret = nt35510_send_long(nt, dsi, NT35510_P1_SET_GAMMA_BLUE_NEG,
				NT35510_P1_GAMMA_LEN,
				nt->conf->gamma_corr_neg_b);
	if (ret)
		return ret;

	 
	ret = nt35510_send_long(nt, dsi, MCS_CMD_MAUCCTR,
				ARRAY_SIZE(nt35510_mauc_select_page_0),
				nt35510_mauc_select_page_0);
	if (ret)
		return ret;

	ret = nt35510_setup_display(nt);
	if (ret)
		return ret;

	return 0;
}

static int nt35510_power_off(struct nt35510 *nt)
{
	int ret;

	ret = regulator_bulk_disable(ARRAY_SIZE(nt->supplies), nt->supplies);
	if (ret)
		return ret;

	if (nt->reset_gpio)
		gpiod_set_value(nt->reset_gpio, 1);

	return 0;
}

static int nt35510_unprepare(struct drm_panel *panel)
{
	struct nt35510 *nt = panel_to_nt35510(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	int ret;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret) {
		dev_err(nt->dev, "failed to turn display off (%d)\n", ret);
		return ret;
	}
	usleep_range(10000, 20000);

	 
	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret) {
		dev_err(nt->dev, "failed to enter sleep mode (%d)\n", ret);
		return ret;
	}

	 
	usleep_range(5000, 10000);

	ret = nt35510_power_off(nt);
	if (ret)
		return ret;

	return 0;
}

static int nt35510_prepare(struct drm_panel *panel)
{
	struct nt35510 *nt = panel_to_nt35510(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(nt->dev);
	int ret;

	ret = nt35510_power_on(nt);
	if (ret)
		return ret;

	 
	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret) {
		dev_err(nt->dev, "failed to exit sleep mode (%d)\n", ret);
		return ret;
	}
	 
	usleep_range(120000, 150000);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret) {
		dev_err(nt->dev, "failed to turn display on (%d)\n", ret);
		return ret;
	}
	 
	usleep_range(10000, 20000);

	return 0;
}

static int nt35510_get_modes(struct drm_panel *panel,
			     struct drm_connector *connector)
{
	struct nt35510 *nt = panel_to_nt35510(panel);
	struct drm_display_mode *mode;
	struct drm_display_info *info;

	info = &connector->display_info;
	info->width_mm = nt->conf->width_mm;
	info->height_mm = nt->conf->height_mm;
	mode = drm_mode_duplicate(connector->dev, &nt->conf->mode);
	if (!mode) {
		dev_err(panel->dev, "bad mode or failed to add mode\n");
		return -EINVAL;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	mode->width_mm = nt->conf->width_mm;
	mode->height_mm = nt->conf->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;  
}

static const struct drm_panel_funcs nt35510_drm_funcs = {
	.unprepare = nt35510_unprepare,
	.prepare = nt35510_prepare,
	.get_modes = nt35510_get_modes,
};

static int nt35510_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct nt35510 *nt;
	int ret;

	nt = devm_kzalloc(dev, sizeof(struct nt35510), GFP_KERNEL);
	if (!nt)
		return -ENOMEM;
	mipi_dsi_set_drvdata(dsi, nt);
	nt->dev = dev;

	dsi->lanes = 2;
	dsi->format = MIPI_DSI_FMT_RGB888;
	 
	dsi->hs_rate = 349440000;
	dsi->lp_rate = 9600000;
	dsi->mode_flags = MIPI_DSI_CLOCK_NON_CONTINUOUS;

	 
	nt->conf = of_device_get_match_data(dev);
	if (!nt->conf) {
		dev_err(dev, "missing device configuration\n");
		return -ENODEV;
	}

	nt->supplies[0].supply = "vdd";  
	nt->supplies[1].supply = "vddi";  
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(nt->supplies),
				      nt->supplies);
	if (ret < 0)
		return ret;
	ret = regulator_set_voltage(nt->supplies[0].consumer,
				    2300000, 4800000);
	if (ret)
		return ret;
	ret = regulator_set_voltage(nt->supplies[1].consumer,
				    1650000, 3300000);
	if (ret)
		return ret;

	nt->reset_gpio = devm_gpiod_get_optional(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(nt->reset_gpio)) {
		dev_err(dev, "error getting RESET GPIO\n");
		return PTR_ERR(nt->reset_gpio);
	}

	drm_panel_init(&nt->panel, dev, &nt35510_drm_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	 
	ret = drm_panel_of_backlight(&nt->panel);
	if (ret) {
		dev_err(dev, "error getting external backlight %d\n", ret);
		return ret;
	}
	if (!nt->panel.backlight) {
		struct backlight_device *bl;

		bl = devm_backlight_device_register(dev, "nt35510", dev, nt,
						    &nt35510_bl_ops, NULL);
		if (IS_ERR(bl)) {
			dev_err(dev, "failed to register backlight device\n");
			return PTR_ERR(bl);
		}
		bl->props.max_brightness = 255;
		bl->props.brightness = 255;
		bl->props.power = FB_BLANK_POWERDOWN;
		nt->panel.backlight = bl;
	}

	drm_panel_add(&nt->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&nt->panel);

	return 0;
}

static void nt35510_remove(struct mipi_dsi_device *dsi)
{
	struct nt35510 *nt = mipi_dsi_get_drvdata(dsi);
	int ret;

	mipi_dsi_detach(dsi);
	 
	ret = nt35510_power_off(nt);
	if (ret)
		dev_err(&dsi->dev, "Failed to power off\n");

	drm_panel_remove(&nt->panel);
}

 
#define NT35510_GAMMA_POS_DEFAULT 0x00, 0x01, 0x00, 0x43, 0x00, \
		0x6B, 0x00, 0x87, 0x00, 0xA3, 0x00, 0xCE, 0x00, 0xF1, 0x01, \
		0x27, 0x01, 0x53, 0x01, 0x98, 0x01, 0xCE, 0x02, 0x22, 0x02, \
		0x83, 0x02, 0x78, 0x02, 0x9E, 0x02, 0xDD, 0x03, 0x00, 0x03, \
		0x2E, 0x03, 0x54, 0x03, 0x7F, 0x03, 0x95, 0x03, 0xB3, 0x03, \
		0xC2, 0x03, 0xE1, 0x03, 0xF1, 0x03, 0xFE

#define NT35510_GAMMA_NEG_DEFAULT 0x00, 0x01, 0x00, 0x43, 0x00, \
		0x6B, 0x00, 0x87, 0x00, 0xA3, 0x00, 0xCE, 0x00, 0xF1, 0x01, \
		0x27, 0x01, 0x53, 0x01, 0x98, 0x01, 0xCE, 0x02, 0x22, 0x02, \
		0x43, 0x02, 0x50, 0x02, 0x9E, 0x02, 0xDD, 0x03, 0x00, 0x03, \
		0x2E, 0x03, 0x54, 0x03, 0x7F, 0x03, 0x95, 0x03, 0xB3, 0x03, \
		0xC2, 0x03, 0xE1, 0x03, 0xF1, 0x03, 0xFE

 
static const struct nt35510_config nt35510_hydis_hva40wv1 = {
	.width_mm = 52,
	.height_mm = 86,
	 
	.mode = {
		 
		.clock = 20000,
		.hdisplay = 480,
		.hsync_start = 480 + 2,  
		.hsync_end = 480 + 2 + 0,  
		.htotal = 480 + 2 + 0 + 5,  
		.vdisplay = 800,
		.vsync_start = 800 + 2,  
		.vsync_end = 800 + 2 + 0,  
		.vtotal = 800 + 2 + 0 + 5,  
		.flags = 0,
	},
	 
	.avdd = { 0x09, 0x09, 0x09 },
	 
	.bt1ctr = { 0x34, 0x34, 0x34 },
	 
	.avee = { 0x09, 0x09, 0x09 },
	 
	.bt2ctr = { 0x24, 0x24, 0x24 },
	 
	.vgh = { 0x05, 0x05, 0x05 },
	 
	.bt4ctr = { 0x24, 0x24, 0x24 },
	 
	.vgl = { 0x0B, 0x0B, 0x0B },
	 
	.bt5ctr = { 0x24, 0x24, 0x24 },
	 
	.vgp = { 0x00, 0xA3, 0x00 },
	 
	.vgn = { 0x00, 0xA3, 0x00 },
	 
	.sdeqctr = { 0x01, 0x05, 0x05, 0x05 },
	 
	.sdvpctr = 0x01,
	 
	.t1 = 0x0184,
	 
	.vbp = 7,
	 
	.vfp = 50,
	 
	.psel = 0,
	 
	.dpmctr12 = { 0x03, 0x00, 0x00, },
	 
	.gamma_corr_pos_r = { NT35510_GAMMA_POS_DEFAULT },
	.gamma_corr_pos_g = { NT35510_GAMMA_POS_DEFAULT },
	.gamma_corr_pos_b = { NT35510_GAMMA_POS_DEFAULT },
	.gamma_corr_neg_r = { NT35510_GAMMA_NEG_DEFAULT },
	.gamma_corr_neg_g = { NT35510_GAMMA_NEG_DEFAULT },
	.gamma_corr_neg_b = { NT35510_GAMMA_NEG_DEFAULT },
};

static const struct of_device_id nt35510_of_match[] = {
	{
		.compatible = "hydis,hva40wv1",
		.data = &nt35510_hydis_hva40wv1,
	},
	{ }
};
MODULE_DEVICE_TABLE(of, nt35510_of_match);

static struct mipi_dsi_driver nt35510_driver = {
	.probe = nt35510_probe,
	.remove = nt35510_remove,
	.driver = {
		.name = "panel-novatek-nt35510",
		.of_match_table = nt35510_of_match,
	},
};
module_mipi_dsi_driver(nt35510_driver);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("NT35510-based panel driver");
MODULE_LICENSE("GPL v2");
