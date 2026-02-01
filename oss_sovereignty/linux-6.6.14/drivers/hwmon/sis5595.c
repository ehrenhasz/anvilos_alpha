
 

 

#define DRIVER_NAME "sis5595"
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/acpi.h>
#include <linux/io.h>

 
static u16 force_addr;
module_param(force_addr, ushort, 0);
MODULE_PARM_DESC(force_addr,
		 "Initialize the base address of the sensors");

static struct platform_device *pdev;

 

 
#define SIS5595_EXTENT 8
 
#define SIS5595_BASE_REG 0x68
#define SIS5595_PIN_REG 0x7A
#define SIS5595_ENABLE_REG 0x7B

 
#define SIS5595_ADDR_REG_OFFSET 5
#define SIS5595_DATA_REG_OFFSET 6

 
#define SIS5595_REG_IN_MAX(nr) (0x2b + (nr) * 2)
#define SIS5595_REG_IN_MIN(nr) (0x2c + (nr) * 2)
#define SIS5595_REG_IN(nr) (0x20 + (nr))

#define SIS5595_REG_FAN_MIN(nr) (0x3b + (nr))
#define SIS5595_REG_FAN(nr) (0x28 + (nr))

 

#define REV2MIN	0xb0
#define SIS5595_REG_TEMP	(((data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN(4) : 0x27)
#define SIS5595_REG_TEMP_OVER	(((data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN_MAX(4) : 0x39)
#define SIS5595_REG_TEMP_HYST	(((data->revision) >= REV2MIN) ? \
					SIS5595_REG_IN_MIN(4) : 0x3a)

#define SIS5595_REG_CONFIG 0x40
#define SIS5595_REG_ALARM1 0x41
#define SIS5595_REG_ALARM2 0x42
#define SIS5595_REG_FANDIV 0x47

 

 
static inline u8 IN_TO_REG(unsigned long val)
{
	unsigned long nval = clamp_val(val, 0, 4080);
	return (nval + 8) / 16;
}
#define IN_FROM_REG(val) ((val) *  16)

static inline u8 FAN_TO_REG(long rpm, int div)
{
	if (rpm <= 0)
		return 255;
	if (rpm > 1350000)
		return 1;
	return clamp_val((1350000 + rpm * div / 2) / (rpm * div), 1, 254);
}

static inline int FAN_FROM_REG(u8 val, int div)
{
	return val == 0 ? -1 : val == 255 ? 0 : 1350000 / (val * div);
}

 
static inline int TEMP_FROM_REG(s8 val)
{
	return val * 830 + 52120;
}
static inline s8 TEMP_TO_REG(long val)
{
	int nval = clamp_val(val, -54120, 157530) ;
	return nval < 0 ? (nval - 5212 - 415) / 830 : (nval - 5212 + 415) / 830;
}

 
static inline u8 DIV_TO_REG(int val)
{
	return val == 8 ? 3 : val == 4 ? 2 : val == 1 ? 0 : 1;
}
#define DIV_FROM_REG(val) (1 << (val))

 
struct sis5595_data {
	unsigned short addr;
	const char *name;
	struct device *hwmon_dev;
	struct mutex lock;

	struct mutex update_lock;
	bool valid;		 
	unsigned long last_updated;	 
	char maxins;		 
	u8 revision;		 

	u8 in[5];		 
	u8 in_max[5];		 
	u8 in_min[5];		 
	u8 fan[2];		 
	u8 fan_min[2];		 
	s8 temp;		 
	s8 temp_over;		 
	s8 temp_hyst;		 
	u8 fan_div[2];		 
	u16 alarms;		 
};

static struct pci_dev *s_bridge;	 

 
static int sis5595_read_value(struct sis5595_data *data, u8 reg)
{
	int res;

	mutex_lock(&data->lock);
	outb_p(reg, data->addr + SIS5595_ADDR_REG_OFFSET);
	res = inb_p(data->addr + SIS5595_DATA_REG_OFFSET);
	mutex_unlock(&data->lock);
	return res;
}

static void sis5595_write_value(struct sis5595_data *data, u8 reg, u8 value)
{
	mutex_lock(&data->lock);
	outb_p(reg, data->addr + SIS5595_ADDR_REG_OFFSET);
	outb_p(value, data->addr + SIS5595_DATA_REG_OFFSET);
	mutex_unlock(&data->lock);
}

static struct sis5595_data *sis5595_update_device(struct device *dev)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	int i;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + HZ + HZ / 2)
	    || !data->valid) {

		for (i = 0; i <= data->maxins; i++) {
			data->in[i] =
			    sis5595_read_value(data, SIS5595_REG_IN(i));
			data->in_min[i] =
			    sis5595_read_value(data,
					       SIS5595_REG_IN_MIN(i));
			data->in_max[i] =
			    sis5595_read_value(data,
					       SIS5595_REG_IN_MAX(i));
		}
		for (i = 0; i < 2; i++) {
			data->fan[i] =
			    sis5595_read_value(data, SIS5595_REG_FAN(i));
			data->fan_min[i] =
			    sis5595_read_value(data,
					       SIS5595_REG_FAN_MIN(i));
		}
		if (data->maxins == 3) {
			data->temp =
			    sis5595_read_value(data, SIS5595_REG_TEMP);
			data->temp_over =
			    sis5595_read_value(data, SIS5595_REG_TEMP_OVER);
			data->temp_hyst =
			    sis5595_read_value(data, SIS5595_REG_TEMP_HYST);
		}
		i = sis5595_read_value(data, SIS5595_REG_FANDIV);
		data->fan_div[0] = (i >> 4) & 0x03;
		data->fan_div[1] = i >> 6;
		data->alarms =
		    sis5595_read_value(data, SIS5595_REG_ALARM1) |
		    (sis5595_read_value(data, SIS5595_REG_ALARM2) << 8);
		data->last_updated = jiffies;
		data->valid = true;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

 
static ssize_t in_show(struct device *dev, struct device_attribute *da,
		       char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", IN_FROM_REG(data->in[nr]));
}

static ssize_t in_min_show(struct device *dev, struct device_attribute *da,
			   char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", IN_FROM_REG(data->in_min[nr]));
}

static ssize_t in_max_show(struct device *dev, struct device_attribute *da,
			   char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", IN_FROM_REG(data->in_max[nr]));
}

static ssize_t in_min_store(struct device *dev, struct device_attribute *da,
			    const char *buf, size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_min[nr] = IN_TO_REG(val);
	sis5595_write_value(data, SIS5595_REG_IN_MIN(nr), data->in_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t in_max_store(struct device *dev, struct device_attribute *da,
			    const char *buf, size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->in_max[nr] = IN_TO_REG(val);
	sis5595_write_value(data, SIS5595_REG_IN_MAX(nr), data->in_max[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static SENSOR_DEVICE_ATTR_RO(in0_input, in, 0);
static SENSOR_DEVICE_ATTR_RW(in0_min, in_min, 0);
static SENSOR_DEVICE_ATTR_RW(in0_max, in_max, 0);
static SENSOR_DEVICE_ATTR_RO(in1_input, in, 1);
static SENSOR_DEVICE_ATTR_RW(in1_min, in_min, 1);
static SENSOR_DEVICE_ATTR_RW(in1_max, in_max, 1);
static SENSOR_DEVICE_ATTR_RO(in2_input, in, 2);
static SENSOR_DEVICE_ATTR_RW(in2_min, in_min, 2);
static SENSOR_DEVICE_ATTR_RW(in2_max, in_max, 2);
static SENSOR_DEVICE_ATTR_RO(in3_input, in, 3);
static SENSOR_DEVICE_ATTR_RW(in3_min, in_min, 3);
static SENSOR_DEVICE_ATTR_RW(in3_max, in_max, 3);
static SENSOR_DEVICE_ATTR_RO(in4_input, in, 4);
static SENSOR_DEVICE_ATTR_RW(in4_min, in_min, 4);
static SENSOR_DEVICE_ATTR_RW(in4_max, in_max, 4);

 
static ssize_t temp1_input_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp));
}

static ssize_t temp1_max_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_over));
}

static ssize_t temp1_max_store(struct device *dev,
			       struct device_attribute *attr, const char *buf,
			       size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_over = TEMP_TO_REG(val);
	sis5595_write_value(data, SIS5595_REG_TEMP_OVER, data->temp_over);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t temp1_max_hyst_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	return sprintf(buf, "%d\n", TEMP_FROM_REG(data->temp_hyst));
}

static ssize_t temp1_max_hyst_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	long val;
	int err;

	err = kstrtol(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->temp_hyst = TEMP_TO_REG(val);
	sis5595_write_value(data, SIS5595_REG_TEMP_HYST, data->temp_hyst);
	mutex_unlock(&data->update_lock);
	return count;
}

static DEVICE_ATTR_RO(temp1_input);
static DEVICE_ATTR_RW(temp1_max);
static DEVICE_ATTR_RW(temp1_max_hyst);

 
static ssize_t fan_show(struct device *dev, struct device_attribute *da,
			char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", FAN_FROM_REG(data->fan[nr],
		DIV_FROM_REG(data->fan_div[nr])));
}

static ssize_t fan_min_show(struct device *dev, struct device_attribute *da,
			    char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", FAN_FROM_REG(data->fan_min[nr],
		DIV_FROM_REG(data->fan_div[nr])));
}

static ssize_t fan_min_store(struct device *dev, struct device_attribute *da,
			     const char *buf, size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	data->fan_min[nr] = FAN_TO_REG(val, DIV_FROM_REG(data->fan_div[nr]));
	sis5595_write_value(data, SIS5595_REG_FAN_MIN(nr), data->fan_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static ssize_t fan_div_show(struct device *dev, struct device_attribute *da,
			    char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	return sprintf(buf, "%d\n", DIV_FROM_REG(data->fan_div[nr]));
}

 
static ssize_t fan_div_store(struct device *dev, struct device_attribute *da,
			     const char *buf, size_t count)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int nr = attr->index;
	unsigned long min;
	int reg;
	unsigned long val;
	int err;

	err = kstrtoul(buf, 10, &val);
	if (err)
		return err;

	mutex_lock(&data->update_lock);
	min = FAN_FROM_REG(data->fan_min[nr],
			DIV_FROM_REG(data->fan_div[nr]));
	reg = sis5595_read_value(data, SIS5595_REG_FANDIV);

	switch (val) {
	case 1:
		data->fan_div[nr] = 0;
		break;
	case 2:
		data->fan_div[nr] = 1;
		break;
	case 4:
		data->fan_div[nr] = 2;
		break;
	case 8:
		data->fan_div[nr] = 3;
		break;
	default:
		dev_err(dev,
			"fan_div value %ld not supported. Choose one of 1, 2, 4 or 8!\n",
			val);
		mutex_unlock(&data->update_lock);
		return -EINVAL;
	}

	switch (nr) {
	case 0:
		reg = (reg & 0xcf) | (data->fan_div[nr] << 4);
		break;
	case 1:
		reg = (reg & 0x3f) | (data->fan_div[nr] << 6);
		break;
	}
	sis5595_write_value(data, SIS5595_REG_FANDIV, reg);
	data->fan_min[nr] =
		FAN_TO_REG(min, DIV_FROM_REG(data->fan_div[nr]));
	sis5595_write_value(data, SIS5595_REG_FAN_MIN(nr), data->fan_min[nr]);
	mutex_unlock(&data->update_lock);
	return count;
}

static SENSOR_DEVICE_ATTR_RO(fan1_input, fan, 0);
static SENSOR_DEVICE_ATTR_RW(fan1_min, fan_min, 0);
static SENSOR_DEVICE_ATTR_RW(fan1_div, fan_div, 0);
static SENSOR_DEVICE_ATTR_RO(fan2_input, fan, 1);
static SENSOR_DEVICE_ATTR_RW(fan2_min, fan_min, 1);
static SENSOR_DEVICE_ATTR_RW(fan2_div, fan_div, 1);

 
static ssize_t alarms_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	return sprintf(buf, "%d\n", data->alarms);
}
static DEVICE_ATTR_RO(alarms);

static ssize_t alarm_show(struct device *dev, struct device_attribute *da,
			  char *buf)
{
	struct sis5595_data *data = sis5595_update_device(dev);
	int nr = to_sensor_dev_attr(da)->index;
	return sprintf(buf, "%u\n", (data->alarms >> nr) & 1);
}
static SENSOR_DEVICE_ATTR_RO(in0_alarm, alarm, 0);
static SENSOR_DEVICE_ATTR_RO(in1_alarm, alarm, 1);
static SENSOR_DEVICE_ATTR_RO(in2_alarm, alarm, 2);
static SENSOR_DEVICE_ATTR_RO(in3_alarm, alarm, 3);
static SENSOR_DEVICE_ATTR_RO(in4_alarm, alarm, 15);
static SENSOR_DEVICE_ATTR_RO(fan1_alarm, alarm, 6);
static SENSOR_DEVICE_ATTR_RO(fan2_alarm, alarm, 7);
static SENSOR_DEVICE_ATTR_RO(temp1_alarm, alarm, 15);

static ssize_t name_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	struct sis5595_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%s\n", data->name);
}
static DEVICE_ATTR_RO(name);

static struct attribute *sis5595_attributes[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in0_min.dev_attr.attr,
	&sensor_dev_attr_in0_max.dev_attr.attr,
	&sensor_dev_attr_in0_alarm.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in1_min.dev_attr.attr,
	&sensor_dev_attr_in1_max.dev_attr.attr,
	&sensor_dev_attr_in1_alarm.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in2_min.dev_attr.attr,
	&sensor_dev_attr_in2_max.dev_attr.attr,
	&sensor_dev_attr_in2_alarm.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in3_min.dev_attr.attr,
	&sensor_dev_attr_in3_max.dev_attr.attr,
	&sensor_dev_attr_in3_alarm.dev_attr.attr,

	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan1_min.dev_attr.attr,
	&sensor_dev_attr_fan1_div.dev_attr.attr,
	&sensor_dev_attr_fan1_alarm.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan2_min.dev_attr.attr,
	&sensor_dev_attr_fan2_div.dev_attr.attr,
	&sensor_dev_attr_fan2_alarm.dev_attr.attr,

	&dev_attr_alarms.attr,
	&dev_attr_name.attr,
	NULL
};

static const struct attribute_group sis5595_group = {
	.attrs = sis5595_attributes,
};

static struct attribute *sis5595_attributes_in4[] = {
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in4_min.dev_attr.attr,
	&sensor_dev_attr_in4_max.dev_attr.attr,
	&sensor_dev_attr_in4_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group sis5595_group_in4 = {
	.attrs = sis5595_attributes_in4,
};

static struct attribute *sis5595_attributes_temp1[] = {
	&dev_attr_temp1_input.attr,
	&dev_attr_temp1_max.attr,
	&dev_attr_temp1_max_hyst.attr,
	&sensor_dev_attr_temp1_alarm.dev_attr.attr,
	NULL
};

static const struct attribute_group sis5595_group_temp1 = {
	.attrs = sis5595_attributes_temp1,
};

 
static void sis5595_init_device(struct sis5595_data *data)
{
	u8 config = sis5595_read_value(data, SIS5595_REG_CONFIG);
	if (!(config & 0x01))
		sis5595_write_value(data, SIS5595_REG_CONFIG,
				(config & 0xf7) | 0x01);
}

 
static int sis5595_probe(struct platform_device *pdev)
{
	int err = 0;
	int i;
	struct sis5595_data *data;
	struct resource *res;
	char val;

	 
	res = platform_get_resource(pdev, IORESOURCE_IO, 0);
	if (!devm_request_region(&pdev->dev, res->start, SIS5595_EXTENT,
				 DRIVER_NAME))
		return -EBUSY;

	data = devm_kzalloc(&pdev->dev, sizeof(struct sis5595_data),
			    GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	mutex_init(&data->lock);
	mutex_init(&data->update_lock);
	data->addr = res->start;
	data->name = DRIVER_NAME;
	platform_set_drvdata(pdev, data);

	 
	data->revision = s_bridge->revision;
	 
	data->maxins = 3;
	if (data->revision >= REV2MIN) {
		pci_read_config_byte(s_bridge, SIS5595_PIN_REG, &val);
		if (!(val & 0x80))
			 
			data->maxins = 4;
	}

	 
	sis5595_init_device(data);

	 
	for (i = 0; i < 2; i++) {
		data->fan_min[i] = sis5595_read_value(data,
					SIS5595_REG_FAN_MIN(i));
	}

	 
	err = sysfs_create_group(&pdev->dev.kobj, &sis5595_group);
	if (err)
		return err;
	if (data->maxins == 4) {
		err = sysfs_create_group(&pdev->dev.kobj, &sis5595_group_in4);
		if (err)
			goto exit_remove_files;
	} else {
		err = sysfs_create_group(&pdev->dev.kobj, &sis5595_group_temp1);
		if (err)
			goto exit_remove_files;
	}

	data->hwmon_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove_files;
	}

	return 0;

exit_remove_files:
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group);
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group_in4);
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group_temp1);
	return err;
}

static int sis5595_remove(struct platform_device *pdev)
{
	struct sis5595_data *data = platform_get_drvdata(pdev);

	hwmon_device_unregister(data->hwmon_dev);
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group);
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group_in4);
	sysfs_remove_group(&pdev->dev.kobj, &sis5595_group_temp1);

	return 0;
}

static const struct pci_device_id sis5595_pci_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503) },
	{ 0, }
};

MODULE_DEVICE_TABLE(pci, sis5595_pci_ids);

static int blacklist[] = {
	PCI_DEVICE_ID_SI_540,
	PCI_DEVICE_ID_SI_550,
	PCI_DEVICE_ID_SI_630,
	PCI_DEVICE_ID_SI_645,
	PCI_DEVICE_ID_SI_730,
	PCI_DEVICE_ID_SI_735,
	PCI_DEVICE_ID_SI_5511,  
	PCI_DEVICE_ID_SI_5597,
	PCI_DEVICE_ID_SI_5598,
	0 };

static int sis5595_device_add(unsigned short address)
{
	struct resource res = {
		.start	= address,
		.end	= address + SIS5595_EXTENT - 1,
		.name	= DRIVER_NAME,
		.flags	= IORESOURCE_IO,
	};
	int err;

	err = acpi_check_resource_conflict(&res);
	if (err)
		goto exit;

	pdev = platform_device_alloc(DRIVER_NAME, address);
	if (!pdev) {
		err = -ENOMEM;
		pr_err("Device allocation failed\n");
		goto exit;
	}

	err = platform_device_add_resources(pdev, &res, 1);
	if (err) {
		pr_err("Device resource addition failed (%d)\n", err);
		goto exit_device_put;
	}

	err = platform_device_add(pdev);
	if (err) {
		pr_err("Device addition failed (%d)\n", err);
		goto exit_device_put;
	}

	return 0;

exit_device_put:
	platform_device_put(pdev);
exit:
	return err;
}

static struct platform_driver sis5595_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= sis5595_probe,
	.remove		= sis5595_remove,
};

static int sis5595_pci_probe(struct pci_dev *dev,
				       const struct pci_device_id *id)
{
	u16 address;
	u8 enable;
	int *i, err;

	for (i = blacklist; *i != 0; i++) {
		struct pci_dev *d;
		d = pci_get_device(PCI_VENDOR_ID_SI, *i, NULL);
		if (d) {
			dev_err(&d->dev,
				"Looked for SIS5595 but found unsupported device %.4x\n",
				*i);
			pci_dev_put(d);
			return -ENODEV;
		}
	}

	force_addr &= ~(SIS5595_EXTENT - 1);
	if (force_addr) {
		dev_warn(&dev->dev, "Forcing ISA address 0x%x\n", force_addr);
		pci_write_config_word(dev, SIS5595_BASE_REG, force_addr);
	}

	err = pci_read_config_word(dev, SIS5595_BASE_REG, &address);
	if (err != PCIBIOS_SUCCESSFUL) {
		dev_err(&dev->dev, "Failed to read ISA address\n");
		return -ENODEV;
	}

	address &= ~(SIS5595_EXTENT - 1);
	if (!address) {
		dev_err(&dev->dev,
			"Base address not set - upgrade BIOS or use force_addr=0xaddr\n");
		return -ENODEV;
	}
	if (force_addr && address != force_addr) {
		 
		dev_err(&dev->dev, "Failed to force ISA address\n");
		return -ENODEV;
	}

	err = pci_read_config_byte(dev, SIS5595_ENABLE_REG, &enable);
	if (err != PCIBIOS_SUCCESSFUL) {
		dev_err(&dev->dev, "Failed to read enable register\n");
		return -ENODEV;
	}
	if (!(enable & 0x80)) {
		err = pci_write_config_byte(dev, SIS5595_ENABLE_REG, enable | 0x80);
		if (err != PCIBIOS_SUCCESSFUL)
			goto enable_fail;

		err = pci_read_config_byte(dev, SIS5595_ENABLE_REG, &enable);
		if (err != PCIBIOS_SUCCESSFUL)
			goto enable_fail;

		 
		if (!(enable & 0x80))
			goto enable_fail;
	}

	if (platform_driver_register(&sis5595_driver)) {
		dev_dbg(&dev->dev, "Failed to register sis5595 driver\n");
		goto exit;
	}

	s_bridge = pci_dev_get(dev);
	 
	if (sis5595_device_add(address))
		goto exit_unregister;

	 
	return -ENODEV;

enable_fail:
	dev_err(&dev->dev, "Failed to enable HWM device\n");
	goto exit;

exit_unregister:
	pci_dev_put(dev);
	platform_driver_unregister(&sis5595_driver);
exit:
	return -ENODEV;
}

static struct pci_driver sis5595_pci_driver = {
	.name            = DRIVER_NAME,
	.id_table        = sis5595_pci_ids,
	.probe           = sis5595_pci_probe,
};

static int __init sm_sis5595_init(void)
{
	return pci_register_driver(&sis5595_pci_driver);
}

static void __exit sm_sis5595_exit(void)
{
	pci_unregister_driver(&sis5595_pci_driver);
	if (s_bridge != NULL) {
		platform_device_unregister(pdev);
		platform_driver_unregister(&sis5595_driver);
		pci_dev_put(s_bridge);
		s_bridge = NULL;
	}
}

MODULE_AUTHOR("Aurelien Jarno <aurelien@aurel32.net>");
MODULE_DESCRIPTION("SiS 5595 Sensor device");
MODULE_LICENSE("GPL");

module_init(sm_sis5595_init);
module_exit(sm_sis5595_exit);
