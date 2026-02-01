
 

#include <linux/debugfs.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define CRYSTALCOVE_SPWRSRC_REG		0x1E
#define CRYSTALCOVE_RESETSRC0_REG	0x20
#define CRYSTALCOVE_RESETSRC1_REG	0x21
#define CRYSTALCOVE_WAKESRC_REG		0x22

struct crc_pwrsrc_data {
	struct regmap *regmap;
	struct dentry *debug_dentry;
	unsigned int resetsrc0;
	unsigned int resetsrc1;
	unsigned int wakesrc;
};

static const char * const pwrsrc_pwrsrc_info[] = {
	  "USB",
	  "DC in",
	  "Battery",
	NULL,
};

static const char * const pwrsrc_resetsrc0_info[] = {
	  "SOC reporting a thermal event",
	  "critical PMIC temperature",
	  "critical system temperature",
	  "critical battery temperature",
	  "VSYS under voltage",
	  "VSYS over voltage",
	  "battery removal",
	NULL,
};

static const char * const pwrsrc_resetsrc1_info[] = {
	  "VCRIT threshold",
	  "BATID reporting battery removal",
	  "user pressing the power button",
	NULL,
};

static const char * const pwrsrc_wakesrc_info[] = {
	  "user pressing the power button",
	  "a battery insertion",
	  "a USB charger insertion",
	  "an adapter insertion",
	NULL,
};

static void crc_pwrsrc_log(struct seq_file *seq, const char *prefix,
			   const char * const *info, unsigned int reg_val)
{
	int i;

	for (i = 0; info[i]; i++) {
		if (reg_val & BIT(i))
			seq_printf(seq, "%s by %s\n", prefix, info[i]);
	}
}

static int pwrsrc_show(struct seq_file *seq, void *unused)
{
	struct crc_pwrsrc_data *data = seq->private;
	unsigned int reg_val;
	int ret;

	ret = regmap_read(data->regmap, CRYSTALCOVE_SPWRSRC_REG, &reg_val);
	if (ret)
		return ret;

	crc_pwrsrc_log(seq, "System powered", pwrsrc_pwrsrc_info, reg_val);
	return 0;
}

static int resetsrc_show(struct seq_file *seq, void *unused)
{
	struct crc_pwrsrc_data *data = seq->private;

	crc_pwrsrc_log(seq, "Last shutdown caused", pwrsrc_resetsrc0_info, data->resetsrc0);
	crc_pwrsrc_log(seq, "Last shutdown caused", pwrsrc_resetsrc1_info, data->resetsrc1);
	return 0;
}

static int wakesrc_show(struct seq_file *seq, void *unused)
{
	struct crc_pwrsrc_data *data = seq->private;

	crc_pwrsrc_log(seq, "Last wake caused", pwrsrc_wakesrc_info, data->wakesrc);
	return 0;
}

DEFINE_SHOW_ATTRIBUTE(pwrsrc);
DEFINE_SHOW_ATTRIBUTE(resetsrc);
DEFINE_SHOW_ATTRIBUTE(wakesrc);

static int crc_pwrsrc_read_and_clear(struct crc_pwrsrc_data *data,
				     unsigned int reg, unsigned int *val)
{
	int ret;

	ret = regmap_read(data->regmap, reg, val);
	if (ret)
		return ret;

	return regmap_write(data->regmap, reg, *val);
}

static int crc_pwrsrc_probe(struct platform_device *pdev)
{
	struct intel_soc_pmic *pmic = dev_get_drvdata(pdev->dev.parent);
	struct crc_pwrsrc_data *data;
	int ret;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->regmap = pmic->regmap;

	 
	ret = crc_pwrsrc_read_and_clear(data, CRYSTALCOVE_RESETSRC0_REG, &data->resetsrc0);
	if (ret)
		return ret;

	ret = crc_pwrsrc_read_and_clear(data, CRYSTALCOVE_RESETSRC1_REG, &data->resetsrc1);
	if (ret)
		return ret;

	ret = crc_pwrsrc_read_and_clear(data, CRYSTALCOVE_WAKESRC_REG, &data->wakesrc);
	if (ret)
		return ret;

	data->debug_dentry = debugfs_create_dir(KBUILD_MODNAME, NULL);
	debugfs_create_file("pwrsrc", 0444, data->debug_dentry, data, &pwrsrc_fops);
	debugfs_create_file("resetsrc", 0444, data->debug_dentry, data, &resetsrc_fops);
	debugfs_create_file("wakesrc", 0444, data->debug_dentry, data, &wakesrc_fops);

	platform_set_drvdata(pdev, data);
	return 0;
}

static int crc_pwrsrc_remove(struct platform_device *pdev)
{
	struct crc_pwrsrc_data *data = platform_get_drvdata(pdev);

	debugfs_remove_recursive(data->debug_dentry);
	return 0;
}

static struct platform_driver crc_pwrsrc_driver = {
	.probe = crc_pwrsrc_probe,
	.remove = crc_pwrsrc_remove,
	.driver = {
		.name = "crystal_cove_pwrsrc",
	},
};
module_platform_driver(crc_pwrsrc_driver);

MODULE_ALIAS("platform:crystal_cove_pwrsrc");
MODULE_AUTHOR("Hans de Goede <hdegoede@redhat.com>");
MODULE_DESCRIPTION("Power-source driver for Bay Trail Crystal Cove PMIC");
MODULE_LICENSE("GPL");
