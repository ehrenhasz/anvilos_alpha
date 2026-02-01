
 

#include <linux/bitops.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <video/of_videomode.h>
#include <video/videomode.h>

#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

 
#define SYS_CLCD			0x50

 
#define SYS_CLCD_CLCDID_MASK		(BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12))
#define SYS_CLCD_ID_SANYO_3_8		(0x00 << 8)
#define SYS_CLCD_ID_SHARP_8_4		(0x01 << 8)
#define SYS_CLCD_ID_EPSON_2_2		(0x02 << 8)
#define SYS_CLCD_ID_SANYO_2_5		(0x07 << 8)
#define SYS_CLCD_ID_VGA			(0x1f << 8)

 
#define IB2_CTRL			0x00
#define IB2_CTRL_LCD_SD			BIT(1)  
#define IB2_CTRL_LCD_BL_ON		BIT(0)
#define IB2_CTRL_LCD_MASK		(BIT(0)|BIT(1))

 
struct versatile_panel_type {
	 
	const char *name;
	 
	u32 magic;
	 
	struct drm_display_mode mode;
	 
	u32 bus_flags;
	 
	u32 width_mm;
	 
	u32 height_mm;
	 
	bool ib2;
};

 
struct versatile_panel {
	 
	struct device *dev;
	 
	struct drm_panel panel;
	 
	const struct versatile_panel_type *panel_type;
	 
	struct regmap *map;
	 
	struct regmap *ib2_map;
};

static const struct versatile_panel_type versatile_panels[] = {
	 
	{
		.name = "Sanyo TM38QV67A02A",
		.magic = SYS_CLCD_ID_SANYO_3_8,
		.width_mm = 79,
		.height_mm = 54,
		.mode = {
			.clock = 10000,
			.hdisplay = 320,
			.hsync_start = 320 + 6,
			.hsync_end = 320 + 6 + 6,
			.htotal = 320 + 6 + 6 + 6,
			.vdisplay = 240,
			.vsync_start = 240 + 5,
			.vsync_end = 240 + 5 + 6,
			.vtotal = 240 + 5 + 6 + 5,
			.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
		},
	},
	 
	{
		.name = "Sharp LQ084V1DG21",
		.magic = SYS_CLCD_ID_SHARP_8_4,
		.width_mm = 171,
		.height_mm = 130,
		.mode = {
			.clock = 25000,
			.hdisplay = 640,
			.hsync_start = 640 + 24,
			.hsync_end = 640 + 24 + 96,
			.htotal = 640 + 24 + 96 + 24,
			.vdisplay = 480,
			.vsync_start = 480 + 11,
			.vsync_end = 480 + 11 + 2,
			.vtotal = 480 + 11 + 2 + 32,
			.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
		},
	},
	 
	{
		.name = "Epson L2F50113T00",
		.magic = SYS_CLCD_ID_EPSON_2_2,
		.width_mm = 34,
		.height_mm = 45,
		.mode = {
			.clock = 62500,
			.hdisplay = 176,
			.hsync_start = 176 + 2,
			.hsync_end = 176 + 2 + 3,
			.htotal = 176 + 2 + 3 + 3,
			.vdisplay = 220,
			.vsync_start = 220 + 0,
			.vsync_end = 220 + 0 + 2,
			.vtotal = 220 + 0 + 2 + 1,
			.flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC,
		},
		.bus_flags = DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE,
	},
	 
	{
		.name = "Sanyo ALR252RGT",
		.magic = SYS_CLCD_ID_SANYO_2_5,
		.width_mm = 37,
		.height_mm = 50,
		.mode = {
			.clock = 5400,
			.hdisplay = 240,
			.hsync_start = 240 + 10,
			.hsync_end = 240 + 10 + 10,
			.htotal = 240 + 10 + 10 + 20,
			.vdisplay = 320,
			.vsync_start = 320 + 2,
			.vsync_end = 320 + 2 + 2,
			.vtotal = 320 + 2 + 2 + 2,
			.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
		},
		.bus_flags = DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE,
		.ib2 = true,
	},
};

static inline struct versatile_panel *
to_versatile_panel(struct drm_panel *panel)
{
	return container_of(panel, struct versatile_panel, panel);
}

static int versatile_panel_disable(struct drm_panel *panel)
{
	struct versatile_panel *vpanel = to_versatile_panel(panel);

	 
	if (vpanel->ib2_map) {
		dev_dbg(vpanel->dev, "disable IB2 display\n");
		regmap_update_bits(vpanel->ib2_map,
				   IB2_CTRL,
				   IB2_CTRL_LCD_MASK,
				   IB2_CTRL_LCD_SD);
	}

	return 0;
}

static int versatile_panel_enable(struct drm_panel *panel)
{
	struct versatile_panel *vpanel = to_versatile_panel(panel);

	 
	if (vpanel->ib2_map) {
		dev_dbg(vpanel->dev, "enable IB2 display\n");
		regmap_update_bits(vpanel->ib2_map,
				   IB2_CTRL,
				   IB2_CTRL_LCD_MASK,
				   IB2_CTRL_LCD_BL_ON);
	}

	return 0;
}

static int versatile_panel_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	struct versatile_panel *vpanel = to_versatile_panel(panel);
	struct drm_display_mode *mode;

	connector->display_info.width_mm = vpanel->panel_type->width_mm;
	connector->display_info.height_mm = vpanel->panel_type->height_mm;
	connector->display_info.bus_flags = vpanel->panel_type->bus_flags;

	mode = drm_mode_duplicate(connector->dev, &vpanel->panel_type->mode);
	if (!mode)
		return -ENOMEM;
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;

	mode->width_mm = vpanel->panel_type->width_mm;
	mode->height_mm = vpanel->panel_type->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs versatile_panel_drm_funcs = {
	.disable = versatile_panel_disable,
	.enable = versatile_panel_enable,
	.get_modes = versatile_panel_get_modes,
};

static int versatile_panel_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct versatile_panel *vpanel;
	struct device *parent;
	struct regmap *map;
	int ret;
	u32 val;
	int i;

	parent = dev->parent;
	if (!parent) {
		dev_err(dev, "no parent for versatile panel\n");
		return -ENODEV;
	}
	map = syscon_node_to_regmap(parent->of_node);
	if (IS_ERR(map)) {
		dev_err(dev, "no regmap for versatile panel parent\n");
		return PTR_ERR(map);
	}

	vpanel = devm_kzalloc(dev, sizeof(*vpanel), GFP_KERNEL);
	if (!vpanel)
		return -ENOMEM;

	ret = regmap_read(map, SYS_CLCD, &val);
	if (ret) {
		dev_err(dev, "cannot access syscon regs\n");
		return ret;
	}

	val &= SYS_CLCD_CLCDID_MASK;

	for (i = 0; i < ARRAY_SIZE(versatile_panels); i++) {
		const struct versatile_panel_type *pt;

		pt = &versatile_panels[i];
		if (pt->magic == val) {
			vpanel->panel_type = pt;
			break;
		}
	}

	 
	if (i == ARRAY_SIZE(versatile_panels)) {
		dev_info(dev, "no panel detected\n");
		return -ENODEV;
	}

	dev_info(dev, "detected: %s\n", vpanel->panel_type->name);
	vpanel->dev = dev;
	vpanel->map = map;

	 
	if (vpanel->panel_type->ib2) {
		vpanel->ib2_map = syscon_regmap_lookup_by_compatible(
			"arm,versatile-ib2-syscon");
		if (IS_ERR(vpanel->ib2_map))
			vpanel->ib2_map = NULL;
		else
			dev_info(dev, "panel mounted on IB2 daughterboard\n");
	}

	drm_panel_init(&vpanel->panel, dev, &versatile_panel_drm_funcs,
		       DRM_MODE_CONNECTOR_DPI);

	drm_panel_add(&vpanel->panel);

	return 0;
}

static const struct of_device_id versatile_panel_match[] = {
	{ .compatible = "arm,versatile-tft-panel", },
	{},
};
MODULE_DEVICE_TABLE(of, versatile_panel_match);

static struct platform_driver versatile_panel_driver = {
	.probe		= versatile_panel_probe,
	.driver		= {
		.name	= "versatile-tft-panel",
		.of_match_table = versatile_panel_match,
	},
};
module_platform_driver(versatile_panel_driver);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("ARM Versatile panel driver");
MODULE_LICENSE("GPL v2");
