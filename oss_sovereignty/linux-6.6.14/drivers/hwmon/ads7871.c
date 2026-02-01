
 

 
 
#define REG_LS_BYTE	0  
#define REG_MS_BYTE	1  
#define REG_PGA_VALID	2  
#define REG_AD_CONTROL	3  
#define REG_GAIN_MUX	4  
#define REG_IO_STATE	5  
#define REG_IO_CONTROL	6  
#define REG_OSC_CONTROL	7  
#define REG_SER_CONTROL 24  
#define REG_ID		31  

 
 
#define INST_MODE_BM	(1 << 7)
#define INST_READ_BM	(1 << 6)
#define INST_16BIT_BM	(1 << 5)

 
 
#define MUX_CNV_BV	7
#define MUX_CNV_BM	(1 << MUX_CNV_BV)
#define MUX_M3_BM	(1 << 3)  
#define MUX_G_BV	4  

 
 
#define OSC_OSCR_BM	(1 << 5)
#define OSC_OSCE_BM	(1 << 4)
#define OSC_REFE_BM	(1 << 3)
#define OSC_BUFE_BM	(1 << 2)
#define OSC_R2V_BM	(1 << 1)
#define OSC_RBG_BM	(1 << 0)

#include <linux/module.h>
#include <linux/init.h>
#include <linux/spi/spi.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/delay.h>

#define DEVICE_NAME	"ads7871"

struct ads7871_data {
	struct spi_device *spi;
};

static int ads7871_read_reg8(struct spi_device *spi, int reg)
{
	int ret;
	reg = reg | INST_READ_BM;
	ret = spi_w8r8(spi, reg);
	return ret;
}

static int ads7871_read_reg16(struct spi_device *spi, int reg)
{
	int ret;
	reg = reg | INST_READ_BM | INST_16BIT_BM;
	ret = spi_w8r16(spi, reg);
	return ret;
}

static int ads7871_write_reg8(struct spi_device *spi, int reg, u8 val)
{
	u8 tmp[2] = {reg, val};
	return spi_write(spi, tmp, sizeof(tmp));
}

static ssize_t voltage_show(struct device *dev, struct device_attribute *da,
			    char *buf)
{
	struct ads7871_data *pdata = dev_get_drvdata(dev);
	struct spi_device *spi = pdata->spi;
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	int ret, val, i = 0;
	uint8_t channel, mux_cnv;

	channel = attr->index;
	 
	 
	 
	ads7871_write_reg8(spi, REG_GAIN_MUX,
		(MUX_CNV_BM | MUX_M3_BM | channel));

	ret = ads7871_read_reg8(spi, REG_GAIN_MUX);
	mux_cnv = ((ret & MUX_CNV_BM) >> MUX_CNV_BV);
	 
	while ((i < 2) && mux_cnv) {
		i++;
		ret = ads7871_read_reg8(spi, REG_GAIN_MUX);
		mux_cnv = ((ret & MUX_CNV_BM) >> MUX_CNV_BV);
		msleep_interruptible(1);
	}

	if (mux_cnv == 0) {
		val = ads7871_read_reg16(spi, REG_LS_BYTE);
		 
		val = ((val >> 2) * 25000) / 8192;
		return sprintf(buf, "%d\n", val);
	} else {
		return -1;
	}
}

static SENSOR_DEVICE_ATTR_RO(in0_input, voltage, 0);
static SENSOR_DEVICE_ATTR_RO(in1_input, voltage, 1);
static SENSOR_DEVICE_ATTR_RO(in2_input, voltage, 2);
static SENSOR_DEVICE_ATTR_RO(in3_input, voltage, 3);
static SENSOR_DEVICE_ATTR_RO(in4_input, voltage, 4);
static SENSOR_DEVICE_ATTR_RO(in5_input, voltage, 5);
static SENSOR_DEVICE_ATTR_RO(in6_input, voltage, 6);
static SENSOR_DEVICE_ATTR_RO(in7_input, voltage, 7);

static struct attribute *ads7871_attrs[] = {
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,
	&sensor_dev_attr_in7_input.dev_attr.attr,
	NULL
};

ATTRIBUTE_GROUPS(ads7871);

static int ads7871_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	int ret;
	uint8_t val;
	struct ads7871_data *pdata;
	struct device *hwmon_dev;

	 
	spi->mode = (SPI_MODE_0);
	spi->bits_per_word = 8;
	spi_setup(spi);

	ads7871_write_reg8(spi, REG_SER_CONTROL, 0);
	ads7871_write_reg8(spi, REG_AD_CONTROL, 0);

	val = (OSC_OSCR_BM | OSC_OSCE_BM | OSC_REFE_BM | OSC_BUFE_BM);
	ads7871_write_reg8(spi, REG_OSC_CONTROL, val);
	ret = ads7871_read_reg8(spi, REG_OSC_CONTROL);

	dev_dbg(dev, "REG_OSC_CONTROL write:%x, read:%x\n", val, ret);
	 
	if (val != ret)
		return -ENODEV;

	pdata = devm_kzalloc(dev, sizeof(struct ads7871_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->spi = spi;

	hwmon_dev = devm_hwmon_device_register_with_groups(dev, spi->modalias,
							   pdata,
							   ads7871_groups);
	return PTR_ERR_OR_ZERO(hwmon_dev);
}

static struct spi_driver ads7871_driver = {
	.driver = {
		.name = DEVICE_NAME,
	},
	.probe = ads7871_probe,
};

module_spi_driver(ads7871_driver);

MODULE_AUTHOR("Paul Thomas <pthomas8589@gmail.com>");
MODULE_DESCRIPTION("TI ADS7871 A/D driver");
MODULE_LICENSE("GPL");
