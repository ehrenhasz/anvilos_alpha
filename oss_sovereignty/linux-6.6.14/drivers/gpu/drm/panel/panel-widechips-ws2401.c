
 
#include <drm/drm_mipi_dbi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>

#include <video/mipi_display.h>

#define WS2401_RESCTL			0xb8  
#define WS2401_PSMPS			0xbd  
#define WS2401_NSMPS			0xbe  
#define WS2401_SMPS			0xbf
#define WS2401_BCMODE			0xc1  
#define WS2401_WRBLCTL			0xc3  
#define WS2401_WRDISBV			0xc4  
#define WS2401_WRCTRLD			0xc6  
#define WS2401_WRMIE			0xc7  
#define WS2401_READ_ID1			0xda  
#define WS2401_READ_ID2			0xdb  
#define WS2401_READ_ID3			0xdc  
#define WS2401_GAMMA_R1			0xe7  
#define WS2401_GAMMA_G1			0xe8  
#define WS2401_GAMMA_B1			0xe9  
#define WS2401_GAMMA_R2			0xea  
#define WS2401_GAMMA_G2			0xeb  
#define WS2401_GAMMA_B2			0xec  
#define WS2401_PASSWD1			0xf0  
#define WS2401_DISCTL			0xf2  
#define WS2401_PWRCTL			0xf3  
#define WS2401_VCOMCTL			0xf4  
#define WS2401_SRCCTL			0xf5  
#define WS2401_PANELCTL			0xf6  

static const u8 ws2401_dbi_read_commands[] = {
	WS2401_READ_ID1,
	WS2401_READ_ID2,
	WS2401_READ_ID3,
	0,  
};

 
struct ws2401 {
	 
	struct device *dev;
	 
	struct mipi_dbi dbi;
	 
	struct drm_panel panel;
	 
	u32 width;
	 
	u32 height;
	 
	struct gpio_desc *reset;
	 
	struct regulator_bulk_data regulators[2];
	 
	bool internal_bl;
};

static const struct drm_display_mode lms380kf01_480_800_mode = {
	 
	.clock = 24960,
	.hdisplay = 480,
	.hsync_start = 480 + 8,
	.hsync_end = 480 + 8 + 10,
	.htotal = 480 + 8 + 10 + 8,
	.vdisplay = 800,
	.vsync_start = 800 + 8,
	.vsync_end = 800 + 8 + 2,
	.vtotal = 800 + 8 + 2 + 18,
	.width_mm = 50,
	.height_mm = 84,
	.flags = DRM_MODE_FLAG_NVSYNC | DRM_MODE_FLAG_NHSYNC,
};

static inline struct ws2401 *to_ws2401(struct drm_panel *panel)
{
	return container_of(panel, struct ws2401, panel);
}

static void ws2401_read_mtp_id(struct ws2401 *ws)
{
	struct mipi_dbi *dbi = &ws->dbi;
	u8 id1, id2, id3;
	int ret;

	ret = mipi_dbi_command_read(dbi, WS2401_READ_ID1, &id1);
	if (ret) {
		dev_err(ws->dev, "unable to read MTP ID 1\n");
		return;
	}
	ret = mipi_dbi_command_read(dbi, WS2401_READ_ID2, &id2);
	if (ret) {
		dev_err(ws->dev, "unable to read MTP ID 2\n");
		return;
	}
	ret = mipi_dbi_command_read(dbi, WS2401_READ_ID3, &id3);
	if (ret) {
		dev_err(ws->dev, "unable to read MTP ID 3\n");
		return;
	}
	dev_info(ws->dev, "MTP ID: %02x %02x %02x\n", id1, id2, id3);
}

static int ws2401_power_on(struct ws2401 *ws)
{
	struct mipi_dbi *dbi = &ws->dbi;
	int ret;

	 
	ret = regulator_bulk_enable(ARRAY_SIZE(ws->regulators),
				    ws->regulators);
	if (ret) {
		dev_err(ws->dev, "failed to enable regulators: %d\n", ret);
		return ret;
	}
	msleep(10);

	 
	gpiod_set_value_cansleep(ws->reset, 1);
	usleep_range(1000, 5000);
	 
	gpiod_set_value_cansleep(ws->reset, 0);
	 
	msleep(10);
	dev_dbg(ws->dev, "de-asserted RESET\n");

	 
	mipi_dbi_command(dbi, MIPI_DCS_EXIT_SLEEP_MODE);
	mipi_dbi_command(dbi, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(50);

	 
	mipi_dbi_command(dbi, WS2401_PASSWD1, 0x5a, 0x5a);
	 
	mipi_dbi_command(dbi, WS2401_RESCTL, 0x12);
	 
	mipi_dbi_command(dbi, MIPI_DCS_SET_ADDRESS_MODE, 0x01);
	 
	mipi_dbi_command(dbi, MIPI_DCS_SET_PIXEL_FORMAT, 0x70);
	mipi_dbi_command(dbi, WS2401_SMPS, 0x00, 0x0f);
	mipi_dbi_command(dbi, WS2401_PSMPS, 0x06, 0x03,  
			 0x7e, 0x03, 0x12, 0x37);
	mipi_dbi_command(dbi, WS2401_NSMPS, 0x06, 0x03,  
			 0x7e, 0x02, 0x15, 0x37);
	mipi_dbi_command(dbi, WS2401_SMPS, 0x02, 0x0f);
	mipi_dbi_command(dbi, WS2401_PWRCTL, 0x10, 0xA9, 0x00, 0x01, 0x44,
			 0xb4,	 
			 0x50,	 
			 0x50,	 
			 0x00,
			 0x44);	 
	mipi_dbi_command(dbi, WS2401_DISCTL, 0x01, 0x00, 0x00, 0x00, 0x14,
			 0x16);
	mipi_dbi_command(dbi, WS2401_VCOMCTL, 0x30, 0x53, 0x53);
	mipi_dbi_command(dbi, WS2401_SRCCTL, 0x03, 0x0C, 0x00, 0x00, 0x00,
			 0x01,	 
			 0x01, 0x06, 0x03);
	mipi_dbi_command(dbi, WS2401_PANELCTL, 0x14, 0x00, 0x80, 0x00);
	mipi_dbi_command(dbi, WS2401_WRMIE, 0x01);

	 
	mipi_dbi_command(dbi, WS2401_GAMMA_R1, 0x00,
			 0x5b, 0x42, 0x41, 0x3f, 0x42, 0x3d, 0x38, 0x2e,
			 0x2b, 0x2a, 0x27, 0x22, 0x27, 0x0f, 0x00, 0x00);
	mipi_dbi_command(dbi, WS2401_GAMMA_R2, 0x00,
			 0x5b, 0x42, 0x41, 0x3f, 0x42, 0x3d, 0x38, 0x2e,
			 0x2b, 0x2a, 0x27, 0x22, 0x27, 0x0f, 0x00, 0x00);
	mipi_dbi_command(dbi, WS2401_GAMMA_G1, 0x00,
			 0x59, 0x40, 0x3f, 0x3e, 0x41, 0x3d, 0x39, 0x2f,
			 0x2c, 0x2b, 0x29, 0x25, 0x29, 0x19, 0x08, 0x00);
	mipi_dbi_command(dbi, WS2401_GAMMA_G2, 0x00,
			 0x59, 0x40, 0x3f, 0x3e, 0x41, 0x3d, 0x39, 0x2f,
			 0x2c, 0x2b, 0x29, 0x25, 0x29, 0x19, 0x08, 0x00);
	mipi_dbi_command(dbi, WS2401_GAMMA_B1, 0x00,
			 0x57, 0x3b, 0x3a, 0x3b, 0x3f, 0x3b, 0x38, 0x27,
			 0x38, 0x2a, 0x26, 0x22, 0x34, 0x0c, 0x09, 0x00);
	mipi_dbi_command(dbi, WS2401_GAMMA_B2, 0x00,
			 0x57, 0x3b, 0x3a, 0x3b, 0x3f, 0x3b, 0x38, 0x27,
			 0x38, 0x2a, 0x26, 0x22, 0x34, 0x0c, 0x09, 0x00);

	if (ws->internal_bl) {
		mipi_dbi_command(dbi, WS2401_WRCTRLD, 0x2c);
	} else {
		mipi_dbi_command(dbi, WS2401_WRCTRLD, 0x00);
		 
		mipi_dbi_command(dbi, WS2401_PASSWD1, 0xa5, 0xa5);
	}

	return 0;
}

static int ws2401_power_off(struct ws2401 *ws)
{
	 
	gpiod_set_value_cansleep(ws->reset, 1);
	return regulator_bulk_disable(ARRAY_SIZE(ws->regulators),
				      ws->regulators);
}

static int ws2401_unprepare(struct drm_panel *panel)
{
	struct ws2401 *ws = to_ws2401(panel);
	struct mipi_dbi *dbi = &ws->dbi;

	 
	if (ws->internal_bl)
		mipi_dbi_command(dbi, WS2401_WRCTRLD, 0x00);
	mipi_dbi_command(dbi, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);
	return ws2401_power_off(to_ws2401(panel));
}

static int ws2401_disable(struct drm_panel *panel)
{
	struct ws2401 *ws = to_ws2401(panel);
	struct mipi_dbi *dbi = &ws->dbi;

	mipi_dbi_command(dbi, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(25);

	return 0;
}

static int ws2401_prepare(struct drm_panel *panel)
{
	return ws2401_power_on(to_ws2401(panel));
}

static int ws2401_enable(struct drm_panel *panel)
{
	struct ws2401 *ws = to_ws2401(panel);
	struct mipi_dbi *dbi = &ws->dbi;

	mipi_dbi_command(dbi, MIPI_DCS_SET_DISPLAY_ON);

	return 0;
}

 
static int ws2401_get_modes(struct drm_panel *panel,
			    struct drm_connector *connector)
{
	struct ws2401 *ws = to_ws2401(panel);
	struct drm_display_mode *mode;
	static const u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;

	 
	mode = drm_mode_duplicate(connector->dev, &lms380kf01_480_800_mode);
	if (!mode) {
		dev_err(ws->dev, "failed to add mode\n");
		return -ENOMEM;
	}

	connector->display_info.bpc = 8;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	connector->display_info.bus_flags =
		DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE;
	drm_display_info_set_bus_formats(&connector->display_info,
					 &bus_format, 1);

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs ws2401_drm_funcs = {
	.disable = ws2401_disable,
	.unprepare = ws2401_unprepare,
	.prepare = ws2401_prepare,
	.enable = ws2401_enable,
	.get_modes = ws2401_get_modes,
};

static int ws2401_set_brightness(struct backlight_device *bl)
{
	struct ws2401 *ws = bl_get_data(bl);
	struct mipi_dbi *dbi = &ws->dbi;
	u8 brightness = backlight_get_brightness(bl);

	if (backlight_is_blank(bl)) {
		mipi_dbi_command(dbi, WS2401_WRCTRLD, 0x00);
	} else {
		mipi_dbi_command(dbi, WS2401_WRCTRLD, 0x2c);
		mipi_dbi_command(dbi, WS2401_WRDISBV, brightness);
	}

	return 0;
}

static const struct backlight_ops ws2401_bl_ops = {
	.update_status = ws2401_set_brightness,
};

static const struct backlight_properties ws2401_bl_props = {
	.type = BACKLIGHT_PLATFORM,
	.brightness = 120,
	.max_brightness = U8_MAX,
};

static int ws2401_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ws2401 *ws;
	int ret;

	ws = devm_kzalloc(dev, sizeof(*ws), GFP_KERNEL);
	if (!ws)
		return -ENOMEM;
	ws->dev = dev;

	 
	ws->regulators[0].supply = "vci";
	ws->regulators[1].supply = "vccio";
	ret = devm_regulator_bulk_get(dev,
				      ARRAY_SIZE(ws->regulators),
				      ws->regulators);
	if (ret)
		return dev_err_probe(dev, ret, "failed to get regulators\n");

	ws->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ws->reset)) {
		ret = PTR_ERR(ws->reset);
		return dev_err_probe(dev, ret, "no RESET GPIO\n");
	}

	ret = mipi_dbi_spi_init(spi, &ws->dbi, NULL);
	if (ret)
		return dev_err_probe(dev, ret, "MIPI DBI init failed\n");
	ws->dbi.read_commands = ws2401_dbi_read_commands;

	ws2401_power_on(ws);
	ws2401_read_mtp_id(ws);
	ws2401_power_off(ws);

	drm_panel_init(&ws->panel, dev, &ws2401_drm_funcs,
		       DRM_MODE_CONNECTOR_DPI);

	ret = drm_panel_of_backlight(&ws->panel);
	if (ret)
		return dev_err_probe(dev, ret,
				"failed to get external backlight device\n");

	if (!ws->panel.backlight) {
		dev_dbg(dev, "no external backlight, using internal backlight\n");
		ws->panel.backlight =
			devm_backlight_device_register(dev, "ws2401", dev, ws,
				&ws2401_bl_ops, &ws2401_bl_props);
		if (IS_ERR(ws->panel.backlight))
			return dev_err_probe(dev, PTR_ERR(ws->panel.backlight),
				"failed to register backlight device\n");
	} else {
		dev_dbg(dev, "using external backlight\n");
	}

	spi_set_drvdata(spi, ws);

	drm_panel_add(&ws->panel);
	dev_dbg(dev, "added panel\n");

	return 0;
}

static void ws2401_remove(struct spi_device *spi)
{
	struct ws2401 *ws = spi_get_drvdata(spi);

	drm_panel_remove(&ws->panel);
}

 
static const struct of_device_id ws2401_match[] = {
	{ .compatible = "samsung,lms380kf01", },
	{},
};
MODULE_DEVICE_TABLE(of, ws2401_match);

static const struct spi_device_id ws2401_ids[] = {
	{ "lms380kf01" },
	{ },
};
MODULE_DEVICE_TABLE(spi, ws2401_ids);

static struct spi_driver ws2401_driver = {
	.probe		= ws2401_probe,
	.remove		= ws2401_remove,
	.id_table	= ws2401_ids,
	.driver		= {
		.name	= "ws2401-panel",
		.of_match_table = ws2401_match,
	},
};
module_spi_driver(ws2401_driver);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("Samsung WS2401 panel driver");
MODULE_LICENSE("GPL v2");
