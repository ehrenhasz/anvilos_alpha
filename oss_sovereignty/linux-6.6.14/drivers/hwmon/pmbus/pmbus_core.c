
 

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/pmbus.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/of.h>
#include <linux/thermal.h>
#include "pmbus.h"

 
#define PMBUS_ATTR_ALLOC_SIZE	32
#define PMBUS_NAME_SIZE		24

struct pmbus_sensor {
	struct pmbus_sensor *next;
	char name[PMBUS_NAME_SIZE];	 
	struct device_attribute attribute;
	u8 page;		 
	u8 phase;		 
	u16 reg;		 
	enum pmbus_sensor_classes class;	 
	bool update;		 
	bool convert;		 
	int data;		 
};
#define to_pmbus_sensor(_attr) \
	container_of(_attr, struct pmbus_sensor, attribute)

struct pmbus_boolean {
	char name[PMBUS_NAME_SIZE];	 
	struct sensor_device_attribute attribute;
	struct pmbus_sensor *s1;
	struct pmbus_sensor *s2;
};
#define to_pmbus_boolean(_attr) \
	container_of(_attr, struct pmbus_boolean, attribute)

struct pmbus_label {
	char name[PMBUS_NAME_SIZE];	 
	struct device_attribute attribute;
	char label[PMBUS_NAME_SIZE];	 
};
#define to_pmbus_label(_attr) \
	container_of(_attr, struct pmbus_label, attribute)

 

#define PB_STATUS_MASK	0xffff
#define PB_REG_SHIFT	16
#define PB_REG_MASK	0x3ff
#define PB_PAGE_SHIFT	26
#define PB_PAGE_MASK	0x3f

#define pb_reg_to_index(page, reg, mask)	(((page) << PB_PAGE_SHIFT) | \
						 ((reg) << PB_REG_SHIFT) | (mask))

#define pb_index_to_page(index)			(((index) >> PB_PAGE_SHIFT) & PB_PAGE_MASK)
#define pb_index_to_reg(index)			(((index) >> PB_REG_SHIFT) & PB_REG_MASK)
#define pb_index_to_mask(index)			((index) & PB_STATUS_MASK)

struct pmbus_data {
	struct device *dev;
	struct device *hwmon_dev;
	struct regulator_dev **rdevs;

	u32 flags;		 

	int exponent[PMBUS_PAGES];
				 

	const struct pmbus_driver_info *info;

	int max_attributes;
	int num_attributes;
	struct attribute_group group;
	const struct attribute_group **groups;
	struct dentry *debugfs;		 

	struct pmbus_sensor *sensors;

	struct mutex update_lock;

	bool has_status_word;		 
	int (*read_status)(struct i2c_client *client, int page);

	s16 currpage;	 
	s16 currphase;	 

	int vout_low[PMBUS_PAGES];	 
	int vout_high[PMBUS_PAGES];	 
};

struct pmbus_debugfs_entry {
	struct i2c_client *client;
	u8 page;
	u8 reg;
};

static const int pmbus_fan_rpm_mask[] = {
	PB_FAN_1_RPM,
	PB_FAN_2_RPM,
	PB_FAN_1_RPM,
	PB_FAN_2_RPM,
};

static const int pmbus_fan_config_registers[] = {
	PMBUS_FAN_CONFIG_12,
	PMBUS_FAN_CONFIG_12,
	PMBUS_FAN_CONFIG_34,
	PMBUS_FAN_CONFIG_34
};

static const int pmbus_fan_command_registers[] = {
	PMBUS_FAN_COMMAND_1,
	PMBUS_FAN_COMMAND_2,
	PMBUS_FAN_COMMAND_3,
	PMBUS_FAN_COMMAND_4,
};

void pmbus_clear_cache(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor *sensor;

	for (sensor = data->sensors; sensor; sensor = sensor->next)
		sensor->data = -ENODATA;
}
EXPORT_SYMBOL_NS_GPL(pmbus_clear_cache, PMBUS);

void pmbus_set_update(struct i2c_client *client, u8 reg, bool update)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor *sensor;

	for (sensor = data->sensors; sensor; sensor = sensor->next)
		if (sensor->reg == reg)
			sensor->update = update;
}
EXPORT_SYMBOL_NS_GPL(pmbus_set_update, PMBUS);

int pmbus_set_page(struct i2c_client *client, int page, int phase)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int rv;

	if (page < 0)
		return 0;

	if (!(data->info->func[page] & PMBUS_PAGE_VIRTUAL) &&
	    data->info->pages > 1 && page != data->currpage) {
		rv = i2c_smbus_write_byte_data(client, PMBUS_PAGE, page);
		if (rv < 0)
			return rv;

		rv = i2c_smbus_read_byte_data(client, PMBUS_PAGE);
		if (rv < 0)
			return rv;

		if (rv != page)
			return -EIO;
	}
	data->currpage = page;

	if (data->info->phases[page] && data->currphase != phase &&
	    !(data->info->func[page] & PMBUS_PHASE_VIRTUAL)) {
		rv = i2c_smbus_write_byte_data(client, PMBUS_PHASE,
					       phase);
		if (rv)
			return rv;
	}
	data->currphase = phase;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(pmbus_set_page, PMBUS);

int pmbus_write_byte(struct i2c_client *client, int page, u8 value)
{
	int rv;

	rv = pmbus_set_page(client, page, 0xff);
	if (rv < 0)
		return rv;

	return i2c_smbus_write_byte(client, value);
}
EXPORT_SYMBOL_NS_GPL(pmbus_write_byte, PMBUS);

 
static int _pmbus_write_byte(struct i2c_client *client, int page, u8 value)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->write_byte) {
		status = info->write_byte(client, page, value);
		if (status != -ENODATA)
			return status;
	}
	return pmbus_write_byte(client, page, value);
}

int pmbus_write_word_data(struct i2c_client *client, int page, u8 reg,
			  u16 word)
{
	int rv;

	rv = pmbus_set_page(client, page, 0xff);
	if (rv < 0)
		return rv;

	return i2c_smbus_write_word_data(client, reg, word);
}
EXPORT_SYMBOL_NS_GPL(pmbus_write_word_data, PMBUS);


static int pmbus_write_virt_reg(struct i2c_client *client, int page, int reg,
				u16 word)
{
	int bit;
	int id;
	int rv;

	switch (reg) {
	case PMBUS_VIRT_FAN_TARGET_1 ... PMBUS_VIRT_FAN_TARGET_4:
		id = reg - PMBUS_VIRT_FAN_TARGET_1;
		bit = pmbus_fan_rpm_mask[id];
		rv = pmbus_update_fan(client, page, id, bit, bit, word);
		break;
	default:
		rv = -ENXIO;
		break;
	}

	return rv;
}

 
static int _pmbus_write_word_data(struct i2c_client *client, int page, int reg,
				  u16 word)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->write_word_data) {
		status = info->write_word_data(client, page, reg, word);
		if (status != -ENODATA)
			return status;
	}

	if (reg >= PMBUS_VIRT_BASE)
		return pmbus_write_virt_reg(client, page, reg, word);

	return pmbus_write_word_data(client, page, reg, word);
}

 
static int _pmbus_write_byte_data(struct i2c_client *client, int page, int reg, u8 value)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->write_byte_data) {
		status = info->write_byte_data(client, page, reg, value);
		if (status != -ENODATA)
			return status;
	}
	return pmbus_write_byte_data(client, page, reg, value);
}

 
static int _pmbus_read_byte_data(struct i2c_client *client, int page, int reg)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->read_byte_data) {
		status = info->read_byte_data(client, page, reg);
		if (status != -ENODATA)
			return status;
	}
	return pmbus_read_byte_data(client, page, reg);
}

int pmbus_update_fan(struct i2c_client *client, int page, int id,
		     u8 config, u8 mask, u16 command)
{
	int from;
	int rv;
	u8 to;

	from = _pmbus_read_byte_data(client, page,
				    pmbus_fan_config_registers[id]);
	if (from < 0)
		return from;

	to = (from & ~mask) | (config & mask);
	if (to != from) {
		rv = _pmbus_write_byte_data(client, page,
					   pmbus_fan_config_registers[id], to);
		if (rv < 0)
			return rv;
	}

	return _pmbus_write_word_data(client, page,
				      pmbus_fan_command_registers[id], command);
}
EXPORT_SYMBOL_NS_GPL(pmbus_update_fan, PMBUS);

int pmbus_read_word_data(struct i2c_client *client, int page, int phase, u8 reg)
{
	int rv;

	rv = pmbus_set_page(client, page, phase);
	if (rv < 0)
		return rv;

	return i2c_smbus_read_word_data(client, reg);
}
EXPORT_SYMBOL_NS_GPL(pmbus_read_word_data, PMBUS);

static int pmbus_read_virt_reg(struct i2c_client *client, int page, int reg)
{
	int rv;
	int id;

	switch (reg) {
	case PMBUS_VIRT_FAN_TARGET_1 ... PMBUS_VIRT_FAN_TARGET_4:
		id = reg - PMBUS_VIRT_FAN_TARGET_1;
		rv = pmbus_get_fan_rate_device(client, page, id, rpm);
		break;
	default:
		rv = -ENXIO;
		break;
	}

	return rv;
}

 
static int _pmbus_read_word_data(struct i2c_client *client, int page,
				 int phase, int reg)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	const struct pmbus_driver_info *info = data->info;
	int status;

	if (info->read_word_data) {
		status = info->read_word_data(client, page, phase, reg);
		if (status != -ENODATA)
			return status;
	}

	if (reg >= PMBUS_VIRT_BASE)
		return pmbus_read_virt_reg(client, page, reg);

	return pmbus_read_word_data(client, page, phase, reg);
}

 
static int __pmbus_read_word_data(struct i2c_client *client, int page, int reg)
{
	return _pmbus_read_word_data(client, page, 0xff, reg);
}

int pmbus_read_byte_data(struct i2c_client *client, int page, u8 reg)
{
	int rv;

	rv = pmbus_set_page(client, page, 0xff);
	if (rv < 0)
		return rv;

	return i2c_smbus_read_byte_data(client, reg);
}
EXPORT_SYMBOL_NS_GPL(pmbus_read_byte_data, PMBUS);

int pmbus_write_byte_data(struct i2c_client *client, int page, u8 reg, u8 value)
{
	int rv;

	rv = pmbus_set_page(client, page, 0xff);
	if (rv < 0)
		return rv;

	return i2c_smbus_write_byte_data(client, reg, value);
}
EXPORT_SYMBOL_NS_GPL(pmbus_write_byte_data, PMBUS);

int pmbus_update_byte_data(struct i2c_client *client, int page, u8 reg,
			   u8 mask, u8 value)
{
	unsigned int tmp;
	int rv;

	rv = _pmbus_read_byte_data(client, page, reg);
	if (rv < 0)
		return rv;

	tmp = (rv & ~mask) | (value & mask);

	if (tmp != rv)
		rv = _pmbus_write_byte_data(client, page, reg, tmp);

	return rv;
}
EXPORT_SYMBOL_NS_GPL(pmbus_update_byte_data, PMBUS);

static int pmbus_read_block_data(struct i2c_client *client, int page, u8 reg,
				 char *data_buf)
{
	int rv;

	rv = pmbus_set_page(client, page, 0xff);
	if (rv < 0)
		return rv;

	return i2c_smbus_read_block_data(client, reg, data_buf);
}

static struct pmbus_sensor *pmbus_find_sensor(struct pmbus_data *data, int page,
					      int reg)
{
	struct pmbus_sensor *sensor;

	for (sensor = data->sensors; sensor; sensor = sensor->next) {
		if (sensor->page == page && sensor->reg == reg)
			return sensor;
	}

	return ERR_PTR(-EINVAL);
}

static int pmbus_get_fan_rate(struct i2c_client *client, int page, int id,
			      enum pmbus_fan_mode mode,
			      bool from_cache)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	bool want_rpm, have_rpm;
	struct pmbus_sensor *s;
	int config;
	int reg;

	want_rpm = (mode == rpm);

	if (from_cache) {
		reg = want_rpm ? PMBUS_VIRT_FAN_TARGET_1 : PMBUS_VIRT_PWM_1;
		s = pmbus_find_sensor(data, page, reg + id);
		if (IS_ERR(s))
			return PTR_ERR(s);

		return s->data;
	}

	config = _pmbus_read_byte_data(client, page,
				      pmbus_fan_config_registers[id]);
	if (config < 0)
		return config;

	have_rpm = !!(config & pmbus_fan_rpm_mask[id]);
	if (want_rpm == have_rpm)
		return pmbus_read_word_data(client, page, 0xff,
					    pmbus_fan_command_registers[id]);

	 
	return 0;
}

int pmbus_get_fan_rate_device(struct i2c_client *client, int page, int id,
			      enum pmbus_fan_mode mode)
{
	return pmbus_get_fan_rate(client, page, id, mode, false);
}
EXPORT_SYMBOL_NS_GPL(pmbus_get_fan_rate_device, PMBUS);

int pmbus_get_fan_rate_cached(struct i2c_client *client, int page, int id,
			      enum pmbus_fan_mode mode)
{
	return pmbus_get_fan_rate(client, page, id, mode, true);
}
EXPORT_SYMBOL_NS_GPL(pmbus_get_fan_rate_cached, PMBUS);

static void pmbus_clear_fault_page(struct i2c_client *client, int page)
{
	_pmbus_write_byte(client, page, PMBUS_CLEAR_FAULTS);
}

void pmbus_clear_faults(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < data->info->pages; i++)
		pmbus_clear_fault_page(client, i);
}
EXPORT_SYMBOL_NS_GPL(pmbus_clear_faults, PMBUS);

static int pmbus_check_status_cml(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int status, status2;

	status = data->read_status(client, -1);
	if (status < 0 || (status & PB_STATUS_CML)) {
		status2 = _pmbus_read_byte_data(client, -1, PMBUS_STATUS_CML);
		if (status2 < 0 || (status2 & PB_CML_FAULT_INVALID_COMMAND))
			return -EIO;
	}
	return 0;
}

static bool pmbus_check_register(struct i2c_client *client,
				 int (*func)(struct i2c_client *client,
					     int page, int reg),
				 int page, int reg)
{
	int rv;
	struct pmbus_data *data = i2c_get_clientdata(client);

	rv = func(client, page, reg);
	if (rv >= 0 && !(data->flags & PMBUS_SKIP_STATUS_CHECK))
		rv = pmbus_check_status_cml(client);
	if (rv < 0 && (data->flags & PMBUS_READ_STATUS_AFTER_FAILED_CHECK))
		data->read_status(client, -1);
	if (reg < PMBUS_VIRT_BASE)
		pmbus_clear_fault_page(client, -1);
	return rv >= 0;
}

static bool pmbus_check_status_register(struct i2c_client *client, int page)
{
	int status;
	struct pmbus_data *data = i2c_get_clientdata(client);

	status = data->read_status(client, page);
	if (status >= 0 && !(data->flags & PMBUS_SKIP_STATUS_CHECK) &&
	    (status & PB_STATUS_CML)) {
		status = _pmbus_read_byte_data(client, -1, PMBUS_STATUS_CML);
		if (status < 0 || (status & PB_CML_FAULT_INVALID_COMMAND))
			status = -EIO;
	}

	pmbus_clear_fault_page(client, -1);
	return status >= 0;
}

bool pmbus_check_byte_register(struct i2c_client *client, int page, int reg)
{
	return pmbus_check_register(client, _pmbus_read_byte_data, page, reg);
}
EXPORT_SYMBOL_NS_GPL(pmbus_check_byte_register, PMBUS);

bool pmbus_check_word_register(struct i2c_client *client, int page, int reg)
{
	return pmbus_check_register(client, __pmbus_read_word_data, page, reg);
}
EXPORT_SYMBOL_NS_GPL(pmbus_check_word_register, PMBUS);

static bool __maybe_unused pmbus_check_block_register(struct i2c_client *client,
						      int page, int reg)
{
	int rv;
	struct pmbus_data *data = i2c_get_clientdata(client);
	char data_buf[I2C_SMBUS_BLOCK_MAX + 2];

	rv = pmbus_read_block_data(client, page, reg, data_buf);
	if (rv >= 0 && !(data->flags & PMBUS_SKIP_STATUS_CHECK))
		rv = pmbus_check_status_cml(client);
	if (rv < 0 && (data->flags & PMBUS_READ_STATUS_AFTER_FAILED_CHECK))
		data->read_status(client, -1);
	pmbus_clear_fault_page(client, -1);
	return rv >= 0;
}

const struct pmbus_driver_info *pmbus_get_driver_info(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);

	return data->info;
}
EXPORT_SYMBOL_NS_GPL(pmbus_get_driver_info, PMBUS);

static int pmbus_get_status(struct i2c_client *client, int page, int reg)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int status;

	switch (reg) {
	case PMBUS_STATUS_WORD:
		status = data->read_status(client, page);
		break;
	default:
		status = _pmbus_read_byte_data(client, page, reg);
		break;
	}
	if (status < 0)
		pmbus_clear_faults(client);
	return status;
}

static void pmbus_update_sensor_data(struct i2c_client *client, struct pmbus_sensor *sensor)
{
	if (sensor->data < 0 || sensor->update)
		sensor->data = _pmbus_read_word_data(client, sensor->page,
						     sensor->phase, sensor->reg);
}

 
static long pmbus_reg2data_ieee754(struct pmbus_data *data,
				   struct pmbus_sensor *sensor)
{
	int exponent;
	bool sign;
	long val;

	 
	sign = sensor->data & 0x8000;
	exponent = (sensor->data >> 10) & 0x1f;
	val = sensor->data & 0x3ff;

	if (exponent == 0) {			 
		exponent = -(14 + 10);
	} else if (exponent ==  0x1f) {		 
		exponent = 0;
		val = 65504;
	} else {
		exponent -= (15 + 10);		 
		val |= 0x400;
	}

	 
	if (sensor->class != PSC_FAN)
		val = val * 1000L;

	 
	if (sensor->class == PSC_POWER)
		val = val * 1000L;

	if (exponent >= 0)
		val <<= exponent;
	else
		val >>= -exponent;

	if (sign)
		val = -val;

	return val;
}

 
static s64 pmbus_reg2data_linear(struct pmbus_data *data,
				 struct pmbus_sensor *sensor)
{
	s16 exponent;
	s32 mantissa;
	s64 val;

	if (sensor->class == PSC_VOLTAGE_OUT) {	 
		exponent = data->exponent[sensor->page];
		mantissa = (u16) sensor->data;
	} else {				 
		exponent = ((s16)sensor->data) >> 11;
		mantissa = ((s16)((sensor->data & 0x7ff) << 5)) >> 5;
	}

	val = mantissa;

	 
	if (sensor->class != PSC_FAN)
		val = val * 1000LL;

	 
	if (sensor->class == PSC_POWER)
		val = val * 1000LL;

	if (exponent >= 0)
		val <<= exponent;
	else
		val >>= -exponent;

	return val;
}

 
static s64 pmbus_reg2data_direct(struct pmbus_data *data,
				 struct pmbus_sensor *sensor)
{
	s64 b, val = (s16)sensor->data;
	s32 m, R;

	m = data->info->m[sensor->class];
	b = data->info->b[sensor->class];
	R = data->info->R[sensor->class];

	if (m == 0)
		return 0;

	 
	R = -R;
	 
	if (!(sensor->class == PSC_FAN || sensor->class == PSC_PWM)) {
		R += 3;
		b *= 1000;
	}

	 
	if (sensor->class == PSC_POWER) {
		R += 3;
		b *= 1000;
	}

	while (R > 0) {
		val *= 10;
		R--;
	}
	while (R < 0) {
		val = div_s64(val + 5LL, 10L);   
		R++;
	}

	val = div_s64(val - b, m);
	return val;
}

 
static s64 pmbus_reg2data_vid(struct pmbus_data *data,
			      struct pmbus_sensor *sensor)
{
	long val = sensor->data;
	long rv = 0;

	switch (data->info->vrm_version[sensor->page]) {
	case vr11:
		if (val >= 0x02 && val <= 0xb2)
			rv = DIV_ROUND_CLOSEST(160000 - (val - 2) * 625, 100);
		break;
	case vr12:
		if (val >= 0x01)
			rv = 250 + (val - 1) * 5;
		break;
	case vr13:
		if (val >= 0x01)
			rv = 500 + (val - 1) * 10;
		break;
	case imvp9:
		if (val >= 0x01)
			rv = 200 + (val - 1) * 10;
		break;
	case amd625mv:
		if (val >= 0x0 && val <= 0xd8)
			rv = DIV_ROUND_CLOSEST(155000 - val * 625, 100);
		break;
	}
	return rv;
}

static s64 pmbus_reg2data(struct pmbus_data *data, struct pmbus_sensor *sensor)
{
	s64 val;

	if (!sensor->convert)
		return sensor->data;

	switch (data->info->format[sensor->class]) {
	case direct:
		val = pmbus_reg2data_direct(data, sensor);
		break;
	case vid:
		val = pmbus_reg2data_vid(data, sensor);
		break;
	case ieee754:
		val = pmbus_reg2data_ieee754(data, sensor);
		break;
	case linear:
	default:
		val = pmbus_reg2data_linear(data, sensor);
		break;
	}
	return val;
}

#define MAX_IEEE_MANTISSA	(0x7ff * 1000)
#define MIN_IEEE_MANTISSA	(0x400 * 1000)

static u16 pmbus_data2reg_ieee754(struct pmbus_data *data,
				  struct pmbus_sensor *sensor, long val)
{
	u16 exponent = (15 + 10);
	long mantissa;
	u16 sign = 0;

	 
	if (val == 0)
		return 0;

	if (val < 0) {
		sign = 0x8000;
		val = -val;
	}

	 
	if (sensor->class == PSC_POWER)
		val = DIV_ROUND_CLOSEST(val, 1000L);

	 
	if (sensor->class == PSC_FAN)
		val = val * 1000;

	 
	while (val > MAX_IEEE_MANTISSA && exponent < 30) {
		exponent++;
		val >>= 1;
	}
	 
	while (val < MIN_IEEE_MANTISSA && exponent > 1) {
		exponent--;
		val <<= 1;
	}

	 
	mantissa = DIV_ROUND_CLOSEST(val, 1000);

	 
	if (mantissa > 0x7ff)
		mantissa = 0x7ff;
	else if (mantissa < 0x400)
		mantissa = 0x400;

	 
	return sign | (mantissa & 0x3ff) | ((exponent << 10) & 0x7c00);
}

#define MAX_LIN_MANTISSA	(1023 * 1000)
#define MIN_LIN_MANTISSA	(511 * 1000)

static u16 pmbus_data2reg_linear(struct pmbus_data *data,
				 struct pmbus_sensor *sensor, s64 val)
{
	s16 exponent = 0, mantissa;
	bool negative = false;

	 
	if (val == 0)
		return 0;

	if (sensor->class == PSC_VOLTAGE_OUT) {
		 
		if (val < 0)
			return 0;

		 
		if (data->exponent[sensor->page] < 0)
			val <<= -data->exponent[sensor->page];
		else
			val >>= data->exponent[sensor->page];
		val = DIV_ROUND_CLOSEST_ULL(val, 1000);
		return clamp_val(val, 0, 0xffff);
	}

	if (val < 0) {
		negative = true;
		val = -val;
	}

	 
	if (sensor->class == PSC_POWER)
		val = DIV_ROUND_CLOSEST_ULL(val, 1000);

	 
	if (sensor->class == PSC_FAN)
		val = val * 1000LL;

	 
	while (val >= MAX_LIN_MANTISSA && exponent < 15) {
		exponent++;
		val >>= 1;
	}
	 
	while (val < MIN_LIN_MANTISSA && exponent > -15) {
		exponent--;
		val <<= 1;
	}

	 
	mantissa = clamp_val(DIV_ROUND_CLOSEST_ULL(val, 1000), 0, 0x3ff);

	 
	if (negative)
		mantissa = -mantissa;

	 
	return (mantissa & 0x7ff) | ((exponent << 11) & 0xf800);
}

static u16 pmbus_data2reg_direct(struct pmbus_data *data,
				 struct pmbus_sensor *sensor, s64 val)
{
	s64 b;
	s32 m, R;

	m = data->info->m[sensor->class];
	b = data->info->b[sensor->class];
	R = data->info->R[sensor->class];

	 
	if (sensor->class == PSC_POWER) {
		R -= 3;
		b *= 1000;
	}

	 
	if (!(sensor->class == PSC_FAN || sensor->class == PSC_PWM)) {
		R -= 3;		 
		b *= 1000;
	}
	val = val * m + b;

	while (R > 0) {
		val *= 10;
		R--;
	}
	while (R < 0) {
		val = div_s64(val + 5LL, 10L);   
		R++;
	}

	return (u16)clamp_val(val, S16_MIN, S16_MAX);
}

static u16 pmbus_data2reg_vid(struct pmbus_data *data,
			      struct pmbus_sensor *sensor, s64 val)
{
	val = clamp_val(val, 500, 1600);

	return 2 + DIV_ROUND_CLOSEST_ULL((1600LL - val) * 100LL, 625);
}

static u16 pmbus_data2reg(struct pmbus_data *data,
			  struct pmbus_sensor *sensor, s64 val)
{
	u16 regval;

	if (!sensor->convert)
		return val;

	switch (data->info->format[sensor->class]) {
	case direct:
		regval = pmbus_data2reg_direct(data, sensor, val);
		break;
	case vid:
		regval = pmbus_data2reg_vid(data, sensor, val);
		break;
	case ieee754:
		regval = pmbus_data2reg_ieee754(data, sensor, val);
		break;
	case linear:
	default:
		regval = pmbus_data2reg_linear(data, sensor, val);
		break;
	}
	return regval;
}

 
static int pmbus_get_boolean(struct i2c_client *client, struct pmbus_boolean *b,
			     int index)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor *s1 = b->s1;
	struct pmbus_sensor *s2 = b->s2;
	u16 mask = pb_index_to_mask(index);
	u8 page = pb_index_to_page(index);
	u16 reg = pb_index_to_reg(index);
	int ret, status;
	u16 regval;

	mutex_lock(&data->update_lock);
	status = pmbus_get_status(client, page, reg);
	if (status < 0) {
		ret = status;
		goto unlock;
	}

	if (s1)
		pmbus_update_sensor_data(client, s1);
	if (s2)
		pmbus_update_sensor_data(client, s2);

	regval = status & mask;
	if (regval) {
		ret = _pmbus_write_byte_data(client, page, reg, regval);
		if (ret)
			goto unlock;
	}
	if (s1 && s2) {
		s64 v1, v2;

		if (s1->data < 0) {
			ret = s1->data;
			goto unlock;
		}
		if (s2->data < 0) {
			ret = s2->data;
			goto unlock;
		}

		v1 = pmbus_reg2data(data, s1);
		v2 = pmbus_reg2data(data, s2);
		ret = !!(regval && v1 >= v2);
	} else {
		ret = !!regval;
	}
unlock:
	mutex_unlock(&data->update_lock);
	return ret;
}

static ssize_t pmbus_show_boolean(struct device *dev,
				  struct device_attribute *da, char *buf)
{
	struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
	struct pmbus_boolean *boolean = to_pmbus_boolean(attr);
	struct i2c_client *client = to_i2c_client(dev->parent);
	int val;

	val = pmbus_get_boolean(client, boolean, attr->index);
	if (val < 0)
		return val;
	return sysfs_emit(buf, "%d\n", val);
}

static ssize_t pmbus_show_sensor(struct device *dev,
				 struct device_attribute *devattr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_sensor *sensor = to_pmbus_sensor(devattr);
	struct pmbus_data *data = i2c_get_clientdata(client);
	ssize_t ret;

	mutex_lock(&data->update_lock);
	pmbus_update_sensor_data(client, sensor);
	if (sensor->data < 0)
		ret = sensor->data;
	else
		ret = sysfs_emit(buf, "%lld\n", pmbus_reg2data(data, sensor));
	mutex_unlock(&data->update_lock);
	return ret;
}

static ssize_t pmbus_set_sensor(struct device *dev,
				struct device_attribute *devattr,
				const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor *sensor = to_pmbus_sensor(devattr);
	ssize_t rv = count;
	s64 val;
	int ret;
	u16 regval;

	if (kstrtos64(buf, 10, &val) < 0)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	regval = pmbus_data2reg(data, sensor, val);
	ret = _pmbus_write_word_data(client, sensor->page, sensor->reg, regval);
	if (ret < 0)
		rv = ret;
	else
		sensor->data = -ENODATA;
	mutex_unlock(&data->update_lock);
	return rv;
}

static ssize_t pmbus_show_label(struct device *dev,
				struct device_attribute *da, char *buf)
{
	struct pmbus_label *label = to_pmbus_label(da);

	return sysfs_emit(buf, "%s\n", label->label);
}

static int pmbus_add_attribute(struct pmbus_data *data, struct attribute *attr)
{
	if (data->num_attributes >= data->max_attributes - 1) {
		int new_max_attrs = data->max_attributes + PMBUS_ATTR_ALLOC_SIZE;
		void *new_attrs = devm_krealloc_array(data->dev, data->group.attrs,
						      new_max_attrs, sizeof(void *),
						      GFP_KERNEL);
		if (!new_attrs)
			return -ENOMEM;
		data->group.attrs = new_attrs;
		data->max_attributes = new_max_attrs;
	}

	data->group.attrs[data->num_attributes++] = attr;
	data->group.attrs[data->num_attributes] = NULL;
	return 0;
}

static void pmbus_dev_attr_init(struct device_attribute *dev_attr,
				const char *name,
				umode_t mode,
				ssize_t (*show)(struct device *dev,
						struct device_attribute *attr,
						char *buf),
				ssize_t (*store)(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count))
{
	sysfs_attr_init(&dev_attr->attr);
	dev_attr->attr.name = name;
	dev_attr->attr.mode = mode;
	dev_attr->show = show;
	dev_attr->store = store;
}

static void pmbus_attr_init(struct sensor_device_attribute *a,
			    const char *name,
			    umode_t mode,
			    ssize_t (*show)(struct device *dev,
					    struct device_attribute *attr,
					    char *buf),
			    ssize_t (*store)(struct device *dev,
					     struct device_attribute *attr,
					     const char *buf, size_t count),
			    int idx)
{
	pmbus_dev_attr_init(&a->dev_attr, name, mode, show, store);
	a->index = idx;
}

static int pmbus_add_boolean(struct pmbus_data *data,
			     const char *name, const char *type, int seq,
			     struct pmbus_sensor *s1,
			     struct pmbus_sensor *s2,
			     u8 page, u16 reg, u16 mask)
{
	struct pmbus_boolean *boolean;
	struct sensor_device_attribute *a;

	if (WARN((s1 && !s2) || (!s1 && s2), "Bad s1/s2 parameters\n"))
		return -EINVAL;

	boolean = devm_kzalloc(data->dev, sizeof(*boolean), GFP_KERNEL);
	if (!boolean)
		return -ENOMEM;

	a = &boolean->attribute;

	snprintf(boolean->name, sizeof(boolean->name), "%s%d_%s",
		 name, seq, type);
	boolean->s1 = s1;
	boolean->s2 = s2;
	pmbus_attr_init(a, boolean->name, 0444, pmbus_show_boolean, NULL,
			pb_reg_to_index(page, reg, mask));

	return pmbus_add_attribute(data, &a->dev_attr.attr);
}

 
struct pmbus_thermal_data {
	struct pmbus_data *pmbus_data;
	struct pmbus_sensor *sensor;
};

static int pmbus_thermal_get_temp(struct thermal_zone_device *tz, int *temp)
{
	struct pmbus_thermal_data *tdata = thermal_zone_device_priv(tz);
	struct pmbus_sensor *sensor = tdata->sensor;
	struct pmbus_data *pmbus_data = tdata->pmbus_data;
	struct i2c_client *client = to_i2c_client(pmbus_data->dev);
	struct device *dev = pmbus_data->hwmon_dev;
	int ret = 0;

	if (!dev) {
		 
		*temp = 0;
		return 0;
	}

	mutex_lock(&pmbus_data->update_lock);
	pmbus_update_sensor_data(client, sensor);
	if (sensor->data < 0)
		ret = sensor->data;
	else
		*temp = (int)pmbus_reg2data(pmbus_data, sensor);
	mutex_unlock(&pmbus_data->update_lock);

	return ret;
}

static const struct thermal_zone_device_ops pmbus_thermal_ops = {
	.get_temp = pmbus_thermal_get_temp,
};

static int pmbus_thermal_add_sensor(struct pmbus_data *pmbus_data,
				    struct pmbus_sensor *sensor, int index)
{
	struct device *dev = pmbus_data->dev;
	struct pmbus_thermal_data *tdata;
	struct thermal_zone_device *tzd;

	tdata = devm_kzalloc(dev, sizeof(*tdata), GFP_KERNEL);
	if (!tdata)
		return -ENOMEM;

	tdata->sensor = sensor;
	tdata->pmbus_data = pmbus_data;

	tzd = devm_thermal_of_zone_register(dev, index, tdata,
					    &pmbus_thermal_ops);
	 
	if (IS_ERR(tzd) && (PTR_ERR(tzd) != -ENODEV))
		return PTR_ERR(tzd);

	return 0;
}

static struct pmbus_sensor *pmbus_add_sensor(struct pmbus_data *data,
					     const char *name, const char *type,
					     int seq, int page, int phase,
					     int reg,
					     enum pmbus_sensor_classes class,
					     bool update, bool readonly,
					     bool convert)
{
	struct pmbus_sensor *sensor;
	struct device_attribute *a;

	sensor = devm_kzalloc(data->dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return NULL;
	a = &sensor->attribute;

	if (type)
		snprintf(sensor->name, sizeof(sensor->name), "%s%d_%s",
			 name, seq, type);
	else
		snprintf(sensor->name, sizeof(sensor->name), "%s%d",
			 name, seq);

	if (data->flags & PMBUS_WRITE_PROTECTED)
		readonly = true;

	sensor->page = page;
	sensor->phase = phase;
	sensor->reg = reg;
	sensor->class = class;
	sensor->update = update;
	sensor->convert = convert;
	sensor->data = -ENODATA;
	pmbus_dev_attr_init(a, sensor->name,
			    readonly ? 0444 : 0644,
			    pmbus_show_sensor, pmbus_set_sensor);

	if (pmbus_add_attribute(data, &a->attr))
		return NULL;

	sensor->next = data->sensors;
	data->sensors = sensor;

	 
	if (class == PSC_TEMPERATURE && strcmp(type, "input") == 0)
		pmbus_thermal_add_sensor(data, sensor, seq);

	return sensor;
}

static int pmbus_add_label(struct pmbus_data *data,
			   const char *name, int seq,
			   const char *lstring, int index, int phase)
{
	struct pmbus_label *label;
	struct device_attribute *a;

	label = devm_kzalloc(data->dev, sizeof(*label), GFP_KERNEL);
	if (!label)
		return -ENOMEM;

	a = &label->attribute;

	snprintf(label->name, sizeof(label->name), "%s%d_label", name, seq);
	if (!index) {
		if (phase == 0xff)
			strncpy(label->label, lstring,
				sizeof(label->label) - 1);
		else
			snprintf(label->label, sizeof(label->label), "%s.%d",
				 lstring, phase);
	} else {
		if (phase == 0xff)
			snprintf(label->label, sizeof(label->label), "%s%d",
				 lstring, index);
		else
			snprintf(label->label, sizeof(label->label), "%s%d.%d",
				 lstring, index, phase);
	}

	pmbus_dev_attr_init(a, label->name, 0444, pmbus_show_label, NULL);
	return pmbus_add_attribute(data, &a->attr);
}

 

 
struct pmbus_limit_attr {
	u16 reg;		 
	u16 sbit;		 
	bool update;		 
	bool low;		 
	const char *attr;	 
	const char *alarm;	 
};

 
struct pmbus_sensor_attr {
	u16 reg;			 
	u16 gbit;			 
	u8 nlimit;			 
	enum pmbus_sensor_classes class; 
	const char *label;		 
	bool paged;			 
	bool update;			 
	bool compare;			 
	u32 func;			 
	u32 sfunc;			 
	int sreg;			 
	const struct pmbus_limit_attr *limit; 
};

 
static int pmbus_add_limit_attrs(struct i2c_client *client,
				 struct pmbus_data *data,
				 const struct pmbus_driver_info *info,
				 const char *name, int index, int page,
				 struct pmbus_sensor *base,
				 const struct pmbus_sensor_attr *attr)
{
	const struct pmbus_limit_attr *l = attr->limit;
	int nlimit = attr->nlimit;
	int have_alarm = 0;
	int i, ret;
	struct pmbus_sensor *curr;

	for (i = 0; i < nlimit; i++) {
		if (pmbus_check_word_register(client, page, l->reg)) {
			curr = pmbus_add_sensor(data, name, l->attr, index,
						page, 0xff, l->reg, attr->class,
						attr->update || l->update,
						false, true);
			if (!curr)
				return -ENOMEM;
			if (l->sbit && (info->func[page] & attr->sfunc)) {
				ret = pmbus_add_boolean(data, name,
					l->alarm, index,
					attr->compare ?  l->low ? curr : base
						      : NULL,
					attr->compare ? l->low ? base : curr
						      : NULL,
					page, attr->sreg, l->sbit);
				if (ret)
					return ret;
				have_alarm = 1;
			}
		}
		l++;
	}
	return have_alarm;
}

static int pmbus_add_sensor_attrs_one(struct i2c_client *client,
				      struct pmbus_data *data,
				      const struct pmbus_driver_info *info,
				      const char *name,
				      int index, int page, int phase,
				      const struct pmbus_sensor_attr *attr,
				      bool paged)
{
	struct pmbus_sensor *base;
	bool upper = !!(attr->gbit & 0xff00);	 
	int ret;

	if (attr->label) {
		ret = pmbus_add_label(data, name, index, attr->label,
				      paged ? page + 1 : 0, phase);
		if (ret)
			return ret;
	}
	base = pmbus_add_sensor(data, name, "input", index, page, phase,
				attr->reg, attr->class, true, true, true);
	if (!base)
		return -ENOMEM;
	 
	if (attr->sfunc && phase == 0xff) {
		ret = pmbus_add_limit_attrs(client, data, info, name,
					    index, page, base, attr);
		if (ret < 0)
			return ret;
		 
		if (!ret && attr->gbit &&
		    (!upper || data->has_status_word) &&
		    pmbus_check_status_register(client, page)) {
			ret = pmbus_add_boolean(data, name, "alarm", index,
						NULL, NULL,
						page, PMBUS_STATUS_WORD,
						attr->gbit);
			if (ret)
				return ret;
		}
	}
	return 0;
}

static bool pmbus_sensor_is_paged(const struct pmbus_driver_info *info,
				  const struct pmbus_sensor_attr *attr)
{
	int p;

	if (attr->paged)
		return true;

	 
	for (p = 1; p < info->pages; p++) {
		if (info->func[p] & attr->func)
			return true;
	}
	return false;
}

static int pmbus_add_sensor_attrs(struct i2c_client *client,
				  struct pmbus_data *data,
				  const char *name,
				  const struct pmbus_sensor_attr *attrs,
				  int nattrs)
{
	const struct pmbus_driver_info *info = data->info;
	int index, i;
	int ret;

	index = 1;
	for (i = 0; i < nattrs; i++) {
		int page, pages;
		bool paged = pmbus_sensor_is_paged(info, attrs);

		pages = paged ? info->pages : 1;
		for (page = 0; page < pages; page++) {
			if (info->func[page] & attrs->func) {
				ret = pmbus_add_sensor_attrs_one(client, data, info,
								 name, index, page,
								 0xff, attrs, paged);
				if (ret)
					return ret;
				index++;
			}
			if (info->phases[page]) {
				int phase;

				for (phase = 0; phase < info->phases[page];
				     phase++) {
					if (!(info->pfunc[phase] & attrs->func))
						continue;
					ret = pmbus_add_sensor_attrs_one(client,
						data, info, name, index, page,
						phase, attrs, paged);
					if (ret)
						return ret;
					index++;
				}
			}
		}
		attrs++;
	}
	return 0;
}

static const struct pmbus_limit_attr vin_limit_attrs[] = {
	{
		.reg = PMBUS_VIN_UV_WARN_LIMIT,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_VOLTAGE_UV_WARNING,
	}, {
		.reg = PMBUS_VIN_UV_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_VOLTAGE_UV_FAULT | PB_VOLTAGE_VIN_OFF,
	}, {
		.reg = PMBUS_VIN_OV_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_VOLTAGE_OV_WARNING,
	}, {
		.reg = PMBUS_VIN_OV_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_VOLTAGE_OV_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_VIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_VIN_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_VIN_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_VIN_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_VIN_MIN,
		.attr = "rated_min",
	}, {
		.reg = PMBUS_MFR_VIN_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_limit_attr vmon_limit_attrs[] = {
	{
		.reg = PMBUS_VIRT_VMON_UV_WARN_LIMIT,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_VOLTAGE_UV_WARNING,
	}, {
		.reg = PMBUS_VIRT_VMON_UV_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_VOLTAGE_UV_FAULT,
	}, {
		.reg = PMBUS_VIRT_VMON_OV_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_VOLTAGE_OV_WARNING,
	}, {
		.reg = PMBUS_VIRT_VMON_OV_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_VOLTAGE_OV_FAULT,
	}
};

static const struct pmbus_limit_attr vout_limit_attrs[] = {
	{
		.reg = PMBUS_VOUT_UV_WARN_LIMIT,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_VOLTAGE_UV_WARNING,
	}, {
		.reg = PMBUS_VOUT_UV_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_VOLTAGE_UV_FAULT,
	}, {
		.reg = PMBUS_VOUT_OV_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_VOLTAGE_OV_WARNING,
	}, {
		.reg = PMBUS_VOUT_OV_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_VOLTAGE_OV_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_VOUT_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_VOUT_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_VOUT_MIN,
		.attr = "rated_min",
	}, {
		.reg = PMBUS_MFR_VOUT_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_sensor_attr voltage_attributes[] = {
	{
		.reg = PMBUS_READ_VIN,
		.class = PSC_VOLTAGE_IN,
		.label = "vin",
		.func = PMBUS_HAVE_VIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sreg = PMBUS_STATUS_INPUT,
		.gbit = PB_STATUS_VIN_UV,
		.limit = vin_limit_attrs,
		.nlimit = ARRAY_SIZE(vin_limit_attrs),
	}, {
		.reg = PMBUS_VIRT_READ_VMON,
		.class = PSC_VOLTAGE_IN,
		.label = "vmon",
		.func = PMBUS_HAVE_VMON,
		.sfunc = PMBUS_HAVE_STATUS_VMON,
		.sreg = PMBUS_VIRT_STATUS_VMON,
		.limit = vmon_limit_attrs,
		.nlimit = ARRAY_SIZE(vmon_limit_attrs),
	}, {
		.reg = PMBUS_READ_VCAP,
		.class = PSC_VOLTAGE_IN,
		.label = "vcap",
		.func = PMBUS_HAVE_VCAP,
	}, {
		.reg = PMBUS_READ_VOUT,
		.class = PSC_VOLTAGE_OUT,
		.label = "vout",
		.paged = true,
		.func = PMBUS_HAVE_VOUT,
		.sfunc = PMBUS_HAVE_STATUS_VOUT,
		.sreg = PMBUS_STATUS_VOUT,
		.gbit = PB_STATUS_VOUT_OV,
		.limit = vout_limit_attrs,
		.nlimit = ARRAY_SIZE(vout_limit_attrs),
	}
};

 

static const struct pmbus_limit_attr iin_limit_attrs[] = {
	{
		.reg = PMBUS_IIN_OC_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_IIN_OC_WARNING,
	}, {
		.reg = PMBUS_IIN_OC_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_IIN_OC_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_IIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_IIN_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_IIN_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_IIN_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_IIN_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_limit_attr iout_limit_attrs[] = {
	{
		.reg = PMBUS_IOUT_OC_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_IOUT_OC_WARNING,
	}, {
		.reg = PMBUS_IOUT_UC_FAULT_LIMIT,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_IOUT_UC_FAULT,
	}, {
		.reg = PMBUS_IOUT_OC_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_IOUT_OC_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_MIN,
		.update = true,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_IOUT_MAX,
		.update = true,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_IOUT_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_IOUT_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_sensor_attr current_attributes[] = {
	{
		.reg = PMBUS_READ_IIN,
		.class = PSC_CURRENT_IN,
		.label = "iin",
		.func = PMBUS_HAVE_IIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sreg = PMBUS_STATUS_INPUT,
		.gbit = PB_STATUS_INPUT,
		.limit = iin_limit_attrs,
		.nlimit = ARRAY_SIZE(iin_limit_attrs),
	}, {
		.reg = PMBUS_READ_IOUT,
		.class = PSC_CURRENT_OUT,
		.label = "iout",
		.paged = true,
		.func = PMBUS_HAVE_IOUT,
		.sfunc = PMBUS_HAVE_STATUS_IOUT,
		.sreg = PMBUS_STATUS_IOUT,
		.gbit = PB_STATUS_IOUT_OC,
		.limit = iout_limit_attrs,
		.nlimit = ARRAY_SIZE(iout_limit_attrs),
	}
};

 

static const struct pmbus_limit_attr pin_limit_attrs[] = {
	{
		.reg = PMBUS_PIN_OP_WARN_LIMIT,
		.attr = "max",
		.alarm = "alarm",
		.sbit = PB_PIN_OP_WARNING,
	}, {
		.reg = PMBUS_VIRT_READ_PIN_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_PIN_MIN,
		.update = true,
		.attr = "input_lowest",
	}, {
		.reg = PMBUS_VIRT_READ_PIN_MAX,
		.update = true,
		.attr = "input_highest",
	}, {
		.reg = PMBUS_VIRT_RESET_PIN_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_PIN_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_limit_attr pout_limit_attrs[] = {
	{
		.reg = PMBUS_POUT_MAX,
		.attr = "cap",
		.alarm = "cap_alarm",
		.sbit = PB_POWER_LIMITING,
	}, {
		.reg = PMBUS_POUT_OP_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_POUT_OP_WARNING,
	}, {
		.reg = PMBUS_POUT_OP_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_POUT_OP_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_POUT_AVG,
		.update = true,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_POUT_MIN,
		.update = true,
		.attr = "input_lowest",
	}, {
		.reg = PMBUS_VIRT_READ_POUT_MAX,
		.update = true,
		.attr = "input_highest",
	}, {
		.reg = PMBUS_VIRT_RESET_POUT_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_POUT_MAX,
		.attr = "rated_max",
	},
};

static const struct pmbus_sensor_attr power_attributes[] = {
	{
		.reg = PMBUS_READ_PIN,
		.class = PSC_POWER,
		.label = "pin",
		.func = PMBUS_HAVE_PIN,
		.sfunc = PMBUS_HAVE_STATUS_INPUT,
		.sreg = PMBUS_STATUS_INPUT,
		.gbit = PB_STATUS_INPUT,
		.limit = pin_limit_attrs,
		.nlimit = ARRAY_SIZE(pin_limit_attrs),
	}, {
		.reg = PMBUS_READ_POUT,
		.class = PSC_POWER,
		.label = "pout",
		.paged = true,
		.func = PMBUS_HAVE_POUT,
		.sfunc = PMBUS_HAVE_STATUS_IOUT,
		.sreg = PMBUS_STATUS_IOUT,
		.limit = pout_limit_attrs,
		.nlimit = ARRAY_SIZE(pout_limit_attrs),
	}
};

 

static const struct pmbus_limit_attr temp_limit_attrs[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_MIN,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_AVG,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP_MAX,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_TEMP_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_MAX_TEMP_1,
		.attr = "rated_max",
	},
};

static const struct pmbus_limit_attr temp_limit_attrs2[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_MIN,
		.attr = "lowest",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_AVG,
		.attr = "average",
	}, {
		.reg = PMBUS_VIRT_READ_TEMP2_MAX,
		.attr = "highest",
	}, {
		.reg = PMBUS_VIRT_RESET_TEMP2_HISTORY,
		.attr = "reset_history",
	}, {
		.reg = PMBUS_MFR_MAX_TEMP_2,
		.attr = "rated_max",
	},
};

static const struct pmbus_limit_attr temp_limit_attrs3[] = {
	{
		.reg = PMBUS_UT_WARN_LIMIT,
		.low = true,
		.attr = "min",
		.alarm = "min_alarm",
		.sbit = PB_TEMP_UT_WARNING,
	}, {
		.reg = PMBUS_UT_FAULT_LIMIT,
		.low = true,
		.attr = "lcrit",
		.alarm = "lcrit_alarm",
		.sbit = PB_TEMP_UT_FAULT,
	}, {
		.reg = PMBUS_OT_WARN_LIMIT,
		.attr = "max",
		.alarm = "max_alarm",
		.sbit = PB_TEMP_OT_WARNING,
	}, {
		.reg = PMBUS_OT_FAULT_LIMIT,
		.attr = "crit",
		.alarm = "crit_alarm",
		.sbit = PB_TEMP_OT_FAULT,
	}, {
		.reg = PMBUS_MFR_MAX_TEMP_3,
		.attr = "rated_max",
	},
};

static const struct pmbus_sensor_attr temp_attributes[] = {
	{
		.reg = PMBUS_READ_TEMPERATURE_1,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sreg = PMBUS_STATUS_TEMPERATURE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs,
		.nlimit = ARRAY_SIZE(temp_limit_attrs),
	}, {
		.reg = PMBUS_READ_TEMPERATURE_2,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP2,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sreg = PMBUS_STATUS_TEMPERATURE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs2,
		.nlimit = ARRAY_SIZE(temp_limit_attrs2),
	}, {
		.reg = PMBUS_READ_TEMPERATURE_3,
		.class = PSC_TEMPERATURE,
		.paged = true,
		.update = true,
		.compare = true,
		.func = PMBUS_HAVE_TEMP3,
		.sfunc = PMBUS_HAVE_STATUS_TEMP,
		.sreg = PMBUS_STATUS_TEMPERATURE,
		.gbit = PB_STATUS_TEMPERATURE,
		.limit = temp_limit_attrs3,
		.nlimit = ARRAY_SIZE(temp_limit_attrs3),
	}
};

static const int pmbus_fan_registers[] = {
	PMBUS_READ_FAN_SPEED_1,
	PMBUS_READ_FAN_SPEED_2,
	PMBUS_READ_FAN_SPEED_3,
	PMBUS_READ_FAN_SPEED_4
};

static const int pmbus_fan_status_registers[] = {
	PMBUS_STATUS_FAN_12,
	PMBUS_STATUS_FAN_12,
	PMBUS_STATUS_FAN_34,
	PMBUS_STATUS_FAN_34
};

static const u32 pmbus_fan_flags[] = {
	PMBUS_HAVE_FAN12,
	PMBUS_HAVE_FAN12,
	PMBUS_HAVE_FAN34,
	PMBUS_HAVE_FAN34
};

static const u32 pmbus_fan_status_flags[] = {
	PMBUS_HAVE_STATUS_FAN12,
	PMBUS_HAVE_STATUS_FAN12,
	PMBUS_HAVE_STATUS_FAN34,
	PMBUS_HAVE_STATUS_FAN34
};

 

 
static int pmbus_add_fan_ctrl(struct i2c_client *client,
		struct pmbus_data *data, int index, int page, int id,
		u8 config)
{
	struct pmbus_sensor *sensor;

	sensor = pmbus_add_sensor(data, "fan", "target", index, page,
				  0xff, PMBUS_VIRT_FAN_TARGET_1 + id, PSC_FAN,
				  false, false, true);

	if (!sensor)
		return -ENOMEM;

	if (!((data->info->func[page] & PMBUS_HAVE_PWM12) ||
			(data->info->func[page] & PMBUS_HAVE_PWM34)))
		return 0;

	sensor = pmbus_add_sensor(data, "pwm", NULL, index, page,
				  0xff, PMBUS_VIRT_PWM_1 + id, PSC_PWM,
				  false, false, true);

	if (!sensor)
		return -ENOMEM;

	sensor = pmbus_add_sensor(data, "pwm", "enable", index, page,
				  0xff, PMBUS_VIRT_PWM_ENABLE_1 + id, PSC_PWM,
				  true, false, false);

	if (!sensor)
		return -ENOMEM;

	return 0;
}

static int pmbus_add_fan_attributes(struct i2c_client *client,
				    struct pmbus_data *data)
{
	const struct pmbus_driver_info *info = data->info;
	int index = 1;
	int page;
	int ret;

	for (page = 0; page < info->pages; page++) {
		int f;

		for (f = 0; f < ARRAY_SIZE(pmbus_fan_registers); f++) {
			int regval;

			if (!(info->func[page] & pmbus_fan_flags[f]))
				break;

			if (!pmbus_check_word_register(client, page,
						       pmbus_fan_registers[f]))
				break;

			 
			regval = _pmbus_read_byte_data(client, page,
				pmbus_fan_config_registers[f]);
			if (regval < 0 ||
			    (!(regval & (PB_FAN_1_INSTALLED >> ((f & 1) * 4)))))
				continue;

			if (pmbus_add_sensor(data, "fan", "input", index,
					     page, 0xff, pmbus_fan_registers[f],
					     PSC_FAN, true, true, true) == NULL)
				return -ENOMEM;

			 
			if (pmbus_check_word_register(client, page,
					pmbus_fan_command_registers[f])) {
				ret = pmbus_add_fan_ctrl(client, data, index,
							 page, f, regval);
				if (ret < 0)
					return ret;
			}

			 
			if ((info->func[page] & pmbus_fan_status_flags[f]) &&
			    pmbus_check_byte_register(client,
					page, pmbus_fan_status_registers[f])) {
				int reg;

				if (f > 1)	 
					reg = PMBUS_STATUS_FAN_34;
				else
					reg = PMBUS_STATUS_FAN_12;
				ret = pmbus_add_boolean(data, "fan",
					"alarm", index, NULL, NULL, page, reg,
					PB_FAN_FAN1_WARNING >> (f & 1));
				if (ret)
					return ret;
				ret = pmbus_add_boolean(data, "fan",
					"fault", index, NULL, NULL, page, reg,
					PB_FAN_FAN1_FAULT >> (f & 1));
				if (ret)
					return ret;
			}
			index++;
		}
	}
	return 0;
}

struct pmbus_samples_attr {
	int reg;
	char *name;
};

struct pmbus_samples_reg {
	int page;
	struct pmbus_samples_attr *attr;
	struct device_attribute dev_attr;
};

static struct pmbus_samples_attr pmbus_samples_registers[] = {
	{
		.reg = PMBUS_VIRT_SAMPLES,
		.name = "samples",
	}, {
		.reg = PMBUS_VIRT_IN_SAMPLES,
		.name = "in_samples",
	}, {
		.reg = PMBUS_VIRT_CURR_SAMPLES,
		.name = "curr_samples",
	}, {
		.reg = PMBUS_VIRT_POWER_SAMPLES,
		.name = "power_samples",
	}, {
		.reg = PMBUS_VIRT_TEMP_SAMPLES,
		.name = "temp_samples",
	}
};

#define to_samples_reg(x) container_of(x, struct pmbus_samples_reg, dev_attr)

static ssize_t pmbus_show_samples(struct device *dev,
				  struct device_attribute *devattr, char *buf)
{
	int val;
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_samples_reg *reg = to_samples_reg(devattr);
	struct pmbus_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->update_lock);
	val = _pmbus_read_word_data(client, reg->page, 0xff, reg->attr->reg);
	mutex_unlock(&data->update_lock);
	if (val < 0)
		return val;

	return sysfs_emit(buf, "%d\n", val);
}

static ssize_t pmbus_set_samples(struct device *dev,
				 struct device_attribute *devattr,
				 const char *buf, size_t count)
{
	int ret;
	long val;
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_samples_reg *reg = to_samples_reg(devattr);
	struct pmbus_data *data = i2c_get_clientdata(client);

	if (kstrtol(buf, 0, &val) < 0)
		return -EINVAL;

	mutex_lock(&data->update_lock);
	ret = _pmbus_write_word_data(client, reg->page, reg->attr->reg, val);
	mutex_unlock(&data->update_lock);

	return ret ? : count;
}

static int pmbus_add_samples_attr(struct pmbus_data *data, int page,
				  struct pmbus_samples_attr *attr)
{
	struct pmbus_samples_reg *reg;

	reg = devm_kzalloc(data->dev, sizeof(*reg), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	reg->attr = attr;
	reg->page = page;

	pmbus_dev_attr_init(&reg->dev_attr, attr->name, 0644,
			    pmbus_show_samples, pmbus_set_samples);

	return pmbus_add_attribute(data, &reg->dev_attr.attr);
}

static int pmbus_add_samples_attributes(struct i2c_client *client,
					struct pmbus_data *data)
{
	const struct pmbus_driver_info *info = data->info;
	int s;

	if (!(info->func[0] & PMBUS_HAVE_SAMPLES))
		return 0;

	for (s = 0; s < ARRAY_SIZE(pmbus_samples_registers); s++) {
		struct pmbus_samples_attr *attr;
		int ret;

		attr = &pmbus_samples_registers[s];
		if (!pmbus_check_word_register(client, 0, attr->reg))
			continue;

		ret = pmbus_add_samples_attr(data, 0, attr);
		if (ret)
			return ret;
	}

	return 0;
}

static int pmbus_find_attributes(struct i2c_client *client,
				 struct pmbus_data *data)
{
	int ret;

	 
	ret = pmbus_add_sensor_attrs(client, data, "in", voltage_attributes,
				     ARRAY_SIZE(voltage_attributes));
	if (ret)
		return ret;

	 
	ret = pmbus_add_sensor_attrs(client, data, "curr", current_attributes,
				     ARRAY_SIZE(current_attributes));
	if (ret)
		return ret;

	 
	ret = pmbus_add_sensor_attrs(client, data, "power", power_attributes,
				     ARRAY_SIZE(power_attributes));
	if (ret)
		return ret;

	 
	ret = pmbus_add_sensor_attrs(client, data, "temp", temp_attributes,
				     ARRAY_SIZE(temp_attributes));
	if (ret)
		return ret;

	 
	ret = pmbus_add_fan_attributes(client, data);
	if (ret)
		return ret;

	ret = pmbus_add_samples_attributes(client, data);
	return ret;
}

 
struct pmbus_class_attr_map {
	enum pmbus_sensor_classes class;
	int nattr;
	const struct pmbus_sensor_attr *attr;
};

static const struct pmbus_class_attr_map class_attr_map[] = {
	{
		.class = PSC_VOLTAGE_IN,
		.attr = voltage_attributes,
		.nattr = ARRAY_SIZE(voltage_attributes),
	}, {
		.class = PSC_VOLTAGE_OUT,
		.attr = voltage_attributes,
		.nattr = ARRAY_SIZE(voltage_attributes),
	}, {
		.class = PSC_CURRENT_IN,
		.attr = current_attributes,
		.nattr = ARRAY_SIZE(current_attributes),
	}, {
		.class = PSC_CURRENT_OUT,
		.attr = current_attributes,
		.nattr = ARRAY_SIZE(current_attributes),
	}, {
		.class = PSC_POWER,
		.attr = power_attributes,
		.nattr = ARRAY_SIZE(power_attributes),
	}, {
		.class = PSC_TEMPERATURE,
		.attr = temp_attributes,
		.nattr = ARRAY_SIZE(temp_attributes),
	}
};

 
static int pmbus_read_coefficients(struct i2c_client *client,
				   struct pmbus_driver_info *info,
				   const struct pmbus_sensor_attr *attr)
{
	int rv;
	union i2c_smbus_data data;
	enum pmbus_sensor_classes class = attr->class;
	s8 R;
	s16 m, b;

	data.block[0] = 2;
	data.block[1] = attr->reg;
	data.block[2] = 0x01;

	rv = i2c_smbus_xfer(client->adapter, client->addr, client->flags,
			    I2C_SMBUS_WRITE, PMBUS_COEFFICIENTS,
			    I2C_SMBUS_BLOCK_PROC_CALL, &data);

	if (rv < 0)
		return rv;

	if (data.block[0] != 5)
		return -EIO;

	m = data.block[1] | (data.block[2] << 8);
	b = data.block[3] | (data.block[4] << 8);
	R = data.block[5];
	info->m[class] = m;
	info->b[class] = b;
	info->R[class] = R;

	return rv;
}

static int pmbus_init_coefficients(struct i2c_client *client,
				   struct pmbus_driver_info *info)
{
	int i, n, ret = -EINVAL;
	const struct pmbus_class_attr_map *map;
	const struct pmbus_sensor_attr *attr;

	for (i = 0; i < ARRAY_SIZE(class_attr_map); i++) {
		map = &class_attr_map[i];
		if (info->format[map->class] != direct)
			continue;
		for (n = 0; n < map->nattr; n++) {
			attr = &map->attr[n];
			if (map->class != attr->class)
				continue;
			ret = pmbus_read_coefficients(client, info, attr);
			if (ret >= 0)
				break;
		}
		if (ret < 0) {
			dev_err(&client->dev,
				"No coefficients found for sensor class %d\n",
				map->class);
			return -EINVAL;
		}
	}

	return 0;
}

 
static int pmbus_identify_common(struct i2c_client *client,
				 struct pmbus_data *data, int page)
{
	int vout_mode = -1;

	if (pmbus_check_byte_register(client, page, PMBUS_VOUT_MODE))
		vout_mode = _pmbus_read_byte_data(client, page,
						  PMBUS_VOUT_MODE);
	if (vout_mode >= 0 && vout_mode != 0xff) {
		 
		switch (vout_mode >> 5) {
		case 0:	 
			if (data->info->format[PSC_VOLTAGE_OUT] != linear)
				return -ENODEV;

			data->exponent[page] = ((s8)(vout_mode << 3)) >> 3;
			break;
		case 1:  
			if (data->info->format[PSC_VOLTAGE_OUT] != vid)
				return -ENODEV;
			break;
		case 2:	 
			if (data->info->format[PSC_VOLTAGE_OUT] != direct)
				return -ENODEV;
			break;
		case 3:	 
			if (data->info->format[PSC_VOLTAGE_OUT] != ieee754)
				return -ENODEV;
			break;
		default:
			return -ENODEV;
		}
	}

	return 0;
}

static int pmbus_read_status_byte(struct i2c_client *client, int page)
{
	return _pmbus_read_byte_data(client, page, PMBUS_STATUS_BYTE);
}

static int pmbus_read_status_word(struct i2c_client *client, int page)
{
	return _pmbus_read_word_data(client, page, 0xff, PMBUS_STATUS_WORD);
}

 

static ssize_t pec_show(struct device *dev, struct device_attribute *dummy,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);

	return sysfs_emit(buf, "%d\n", !!(client->flags & I2C_CLIENT_PEC));
}

static ssize_t pec_store(struct device *dev, struct device_attribute *dummy,
			 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	bool enable;
	int err;

	err = kstrtobool(buf, &enable);
	if (err < 0)
		return err;

	if (enable)
		client->flags |= I2C_CLIENT_PEC;
	else
		client->flags &= ~I2C_CLIENT_PEC;

	return count;
}

static DEVICE_ATTR_RW(pec);

static void pmbus_remove_pec(void *dev)
{
	device_remove_file(dev, &dev_attr_pec);
}

static int pmbus_init_common(struct i2c_client *client, struct pmbus_data *data,
			     struct pmbus_driver_info *info)
{
	struct device *dev = &client->dev;
	int page, ret;

	 
	client->flags &= ~I2C_CLIENT_PEC;

	 
	if (!(data->flags & PMBUS_NO_CAPABILITY)) {
		ret = i2c_smbus_read_byte_data(client, PMBUS_CAPABILITY);
		if (ret >= 0 && (ret & PB_CAPABILITY_ERROR_CHECK)) {
			if (i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_PEC))
				client->flags |= I2C_CLIENT_PEC;
		}
	}

	 
	data->read_status = pmbus_read_status_word;
	ret = i2c_smbus_read_word_data(client, PMBUS_STATUS_WORD);
	if (ret < 0 || ret == 0xffff) {
		data->read_status = pmbus_read_status_byte;
		ret = i2c_smbus_read_byte_data(client, PMBUS_STATUS_BYTE);
		if (ret < 0 || ret == 0xff) {
			dev_err(dev, "PMBus status register not found\n");
			return -ENODEV;
		}
	} else {
		data->has_status_word = true;
	}

	 
	if (!(data->flags & PMBUS_NO_WRITE_PROTECT)) {
		ret = i2c_smbus_read_byte_data(client, PMBUS_WRITE_PROTECT);
		if (ret > 0 && (ret & PB_WP_ANY))
			data->flags |= PMBUS_WRITE_PROTECTED | PMBUS_SKIP_STATUS_CHECK;
	}

	if (data->info->pages)
		pmbus_clear_faults(client);
	else
		pmbus_clear_fault_page(client, -1);

	if (info->identify) {
		ret = (*info->identify)(client, info);
		if (ret < 0) {
			dev_err(dev, "Chip identification failed\n");
			return ret;
		}
	}

	if (info->pages <= 0 || info->pages > PMBUS_PAGES) {
		dev_err(dev, "Bad number of PMBus pages: %d\n", info->pages);
		return -ENODEV;
	}

	for (page = 0; page < info->pages; page++) {
		ret = pmbus_identify_common(client, data, page);
		if (ret < 0) {
			dev_err(dev, "Failed to identify chip capabilities\n");
			return ret;
		}
	}

	if (data->flags & PMBUS_USE_COEFFICIENTS_CMD) {
		if (!i2c_check_functionality(client->adapter,
					     I2C_FUNC_SMBUS_BLOCK_PROC_CALL))
			return -ENODEV;

		ret = pmbus_init_coefficients(client, info);
		if (ret < 0)
			return ret;
	}

	if (client->flags & I2C_CLIENT_PEC) {
		 
		ret = device_create_file(dev, &dev_attr_pec);
		if (ret)
			return ret;
		ret = devm_add_action_or_reset(dev, pmbus_remove_pec, dev);
		if (ret)
			return ret;
	}

	return 0;
}

 
struct pmbus_status_assoc {
	int pflag, rflag, eflag;
};

 
struct pmbus_status_category {
	int func;
	int reg;
	const struct pmbus_status_assoc *bits;  
};

static const struct pmbus_status_category __maybe_unused pmbus_status_flag_map[] = {
	{
		.func = PMBUS_HAVE_STATUS_VOUT,
		.reg = PMBUS_STATUS_VOUT,
		.bits = (const struct pmbus_status_assoc[]) {
			{ PB_VOLTAGE_UV_WARNING, REGULATOR_ERROR_UNDER_VOLTAGE_WARN,
			REGULATOR_EVENT_UNDER_VOLTAGE_WARN },
			{ PB_VOLTAGE_UV_FAULT,   REGULATOR_ERROR_UNDER_VOLTAGE,
			REGULATOR_EVENT_UNDER_VOLTAGE },
			{ PB_VOLTAGE_OV_WARNING, REGULATOR_ERROR_OVER_VOLTAGE_WARN,
			REGULATOR_EVENT_OVER_VOLTAGE_WARN },
			{ PB_VOLTAGE_OV_FAULT,   REGULATOR_ERROR_REGULATION_OUT,
			REGULATOR_EVENT_OVER_VOLTAGE_WARN },
			{ },
		},
	}, {
		.func = PMBUS_HAVE_STATUS_IOUT,
		.reg = PMBUS_STATUS_IOUT,
		.bits = (const struct pmbus_status_assoc[]) {
			{ PB_IOUT_OC_WARNING,   REGULATOR_ERROR_OVER_CURRENT_WARN,
			REGULATOR_EVENT_OVER_CURRENT_WARN },
			{ PB_IOUT_OC_FAULT,     REGULATOR_ERROR_OVER_CURRENT,
			REGULATOR_EVENT_OVER_CURRENT },
			{ PB_IOUT_OC_LV_FAULT,  REGULATOR_ERROR_OVER_CURRENT,
			REGULATOR_EVENT_OVER_CURRENT },
			{ },
		},
	}, {
		.func = PMBUS_HAVE_STATUS_TEMP,
		.reg = PMBUS_STATUS_TEMPERATURE,
		.bits = (const struct pmbus_status_assoc[]) {
			{ PB_TEMP_OT_WARNING,    REGULATOR_ERROR_OVER_TEMP_WARN,
			REGULATOR_EVENT_OVER_TEMP_WARN },
			{ PB_TEMP_OT_FAULT,      REGULATOR_ERROR_OVER_TEMP,
			REGULATOR_EVENT_OVER_TEMP },
			{ },
		},
	},
};

static int _pmbus_is_enabled(struct i2c_client *client, u8 page)
{
	int ret;

	ret = _pmbus_read_byte_data(client, page, PMBUS_OPERATION);

	if (ret < 0)
		return ret;

	return !!(ret & PB_OPERATION_CONTROL_ON);
}

static int __maybe_unused pmbus_is_enabled(struct i2c_client *client, u8 page)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&data->update_lock);
	ret = _pmbus_is_enabled(client, page);
	mutex_unlock(&data->update_lock);

	return ret;
}

#define to_dev_attr(_dev_attr) \
	container_of(_dev_attr, struct device_attribute, attr)

static void pmbus_notify(struct pmbus_data *data, int page, int reg, int flags)
{
	int i;

	for (i = 0; i < data->num_attributes; i++) {
		struct device_attribute *da = to_dev_attr(data->group.attrs[i]);
		struct sensor_device_attribute *attr = to_sensor_dev_attr(da);
		int index = attr->index;
		u16 smask = pb_index_to_mask(index);
		u8 spage = pb_index_to_page(index);
		u16 sreg = pb_index_to_reg(index);

		if (reg == sreg && page == spage && (smask & flags)) {
			dev_dbg(data->dev, "sysfs notify: %s", da->attr.name);
			sysfs_notify(&data->dev->kobj, NULL, da->attr.name);
			kobject_uevent(&data->dev->kobj, KOBJ_CHANGE);
			flags &= ~smask;
		}

		if (!flags)
			break;
	}
}

static int _pmbus_get_flags(struct pmbus_data *data, u8 page, unsigned int *flags,
			   unsigned int *event, bool notify)
{
	int i, status;
	const struct pmbus_status_category *cat;
	const struct pmbus_status_assoc *bit;
	struct device *dev = data->dev;
	struct i2c_client *client = to_i2c_client(dev);
	int func = data->info->func[page];

	*flags = 0;
	*event = 0;

	for (i = 0; i < ARRAY_SIZE(pmbus_status_flag_map); i++) {
		cat = &pmbus_status_flag_map[i];
		if (!(func & cat->func))
			continue;

		status = _pmbus_read_byte_data(client, page, cat->reg);
		if (status < 0)
			return status;

		for (bit = cat->bits; bit->pflag; bit++)
			if (status & bit->pflag) {
				*flags |= bit->rflag;
				*event |= bit->eflag;
			}

		if (notify && status)
			pmbus_notify(data, page, cat->reg, status);

	}

	 
	status = pmbus_get_status(client, page, PMBUS_STATUS_WORD);
	if (status < 0)
		return status;

	if (_pmbus_is_enabled(client, page)) {
		if (status & PB_STATUS_OFF) {
			*flags |= REGULATOR_ERROR_FAIL;
			*event |= REGULATOR_EVENT_FAIL;
		}

		if (status & PB_STATUS_POWER_GOOD_N) {
			*flags |= REGULATOR_ERROR_REGULATION_OUT;
			*event |= REGULATOR_EVENT_REGULATION_OUT;
		}
	}
	 
	if (status & PB_STATUS_IOUT_OC) {
		*flags |= REGULATOR_ERROR_OVER_CURRENT;
		*event |= REGULATOR_EVENT_OVER_CURRENT;
	}
	if (status & PB_STATUS_VOUT_OV) {
		*flags |= REGULATOR_ERROR_REGULATION_OUT;
		*event |= REGULATOR_EVENT_FAIL;
	}

	 
	if (!(*flags & (REGULATOR_ERROR_OVER_TEMP | REGULATOR_ERROR_OVER_TEMP_WARN)) &&
	    (status & PB_STATUS_TEMPERATURE)) {
		*flags |= REGULATOR_ERROR_OVER_TEMP_WARN;
		*event |= REGULATOR_EVENT_OVER_TEMP_WARN;
	}


	return 0;
}

static int __maybe_unused pmbus_get_flags(struct pmbus_data *data, u8 page, unsigned int *flags,
					  unsigned int *event, bool notify)
{
	int ret;

	mutex_lock(&data->update_lock);
	ret = _pmbus_get_flags(data, page, flags, event, notify);
	mutex_unlock(&data->update_lock);

	return ret;
}

#if IS_ENABLED(CONFIG_REGULATOR)
static int pmbus_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);

	return pmbus_is_enabled(client, rdev_get_id(rdev));
}

static int _pmbus_regulator_on_off(struct regulator_dev *rdev, bool enable)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	u8 page = rdev_get_id(rdev);
	int ret;

	mutex_lock(&data->update_lock);
	ret = pmbus_update_byte_data(client, page, PMBUS_OPERATION,
				     PB_OPERATION_CONTROL_ON,
				     enable ? PB_OPERATION_CONTROL_ON : 0);
	mutex_unlock(&data->update_lock);

	return ret;
}

static int pmbus_regulator_enable(struct regulator_dev *rdev)
{
	return _pmbus_regulator_on_off(rdev, 1);
}

static int pmbus_regulator_disable(struct regulator_dev *rdev)
{
	return _pmbus_regulator_on_off(rdev, 0);
}

static int pmbus_regulator_get_error_flags(struct regulator_dev *rdev, unsigned int *flags)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	int event;

	return pmbus_get_flags(data, rdev_get_id(rdev), flags, &event, false);
}

static int pmbus_regulator_get_status(struct regulator_dev *rdev)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	u8 page = rdev_get_id(rdev);
	int status, ret;
	int event;

	mutex_lock(&data->update_lock);
	status = pmbus_get_status(client, page, PMBUS_STATUS_WORD);
	if (status < 0) {
		ret = status;
		goto unlock;
	}

	if (status & PB_STATUS_OFF) {
		ret = REGULATOR_STATUS_OFF;
		goto unlock;
	}

	 
	if (!(status & PB_STATUS_POWER_GOOD_N)) {
		ret = REGULATOR_STATUS_ON;
		goto unlock;
	}

	ret = _pmbus_get_flags(data, rdev_get_id(rdev), &status, &event, false);
	if (ret)
		goto unlock;

	if (status & (REGULATOR_ERROR_UNDER_VOLTAGE | REGULATOR_ERROR_OVER_CURRENT |
	   REGULATOR_ERROR_REGULATION_OUT | REGULATOR_ERROR_FAIL | REGULATOR_ERROR_OVER_TEMP)) {
		ret = REGULATOR_STATUS_ERROR;
		goto unlock;
	}

	ret = REGULATOR_STATUS_UNDEFINED;

unlock:
	mutex_unlock(&data->update_lock);
	return ret;
}

static int pmbus_regulator_get_low_margin(struct i2c_client *client, int page)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor s = {
		.page = page,
		.class = PSC_VOLTAGE_OUT,
		.convert = true,
		.data = -1,
	};

	if (data->vout_low[page] < 0) {
		if (pmbus_check_word_register(client, page, PMBUS_MFR_VOUT_MIN))
			s.data = _pmbus_read_word_data(client, page, 0xff,
						       PMBUS_MFR_VOUT_MIN);
		if (s.data < 0) {
			s.data = _pmbus_read_word_data(client, page, 0xff,
						       PMBUS_VOUT_MARGIN_LOW);
			if (s.data < 0)
				return s.data;
		}
		data->vout_low[page] = pmbus_reg2data(data, &s);
	}

	return data->vout_low[page];
}

static int pmbus_regulator_get_high_margin(struct i2c_client *client, int page)
{
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor s = {
		.page = page,
		.class = PSC_VOLTAGE_OUT,
		.convert = true,
		.data = -1,
	};

	if (data->vout_high[page] < 0) {
		if (pmbus_check_word_register(client, page, PMBUS_MFR_VOUT_MAX))
			s.data = _pmbus_read_word_data(client, page, 0xff,
						       PMBUS_MFR_VOUT_MAX);
		if (s.data < 0) {
			s.data = _pmbus_read_word_data(client, page, 0xff,
						       PMBUS_VOUT_MARGIN_HIGH);
			if (s.data < 0)
				return s.data;
		}
		data->vout_high[page] = pmbus_reg2data(data, &s);
	}

	return data->vout_high[page];
}

static int pmbus_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor s = {
		.page = rdev_get_id(rdev),
		.class = PSC_VOLTAGE_OUT,
		.convert = true,
	};

	s.data = _pmbus_read_word_data(client, s.page, 0xff, PMBUS_READ_VOUT);
	if (s.data < 0)
		return s.data;

	return (int)pmbus_reg2data(data, &s) * 1000;  
}

static int pmbus_regulator_set_voltage(struct regulator_dev *rdev, int min_uv,
				       int max_uv, unsigned int *selector)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	struct pmbus_data *data = i2c_get_clientdata(client);
	struct pmbus_sensor s = {
		.page = rdev_get_id(rdev),
		.class = PSC_VOLTAGE_OUT,
		.convert = true,
		.data = -1,
	};
	int val = DIV_ROUND_CLOSEST(min_uv, 1000);  
	int low, high;

	*selector = 0;

	low = pmbus_regulator_get_low_margin(client, s.page);
	if (low < 0)
		return low;

	high = pmbus_regulator_get_high_margin(client, s.page);
	if (high < 0)
		return high;

	 
	if (low > val)
		val = low;
	if (high < val)
		val = high;

	val = pmbus_data2reg(data, &s, val);

	return _pmbus_write_word_data(client, s.page, PMBUS_VOUT_COMMAND, (u16)val);
}

static int pmbus_regulator_list_voltage(struct regulator_dev *rdev,
					 unsigned int selector)
{
	struct device *dev = rdev_get_dev(rdev);
	struct i2c_client *client = to_i2c_client(dev->parent);
	int val, low, high;

	if (selector >= rdev->desc->n_voltages ||
	    selector < rdev->desc->linear_min_sel)
		return -EINVAL;

	selector -= rdev->desc->linear_min_sel;
	val = DIV_ROUND_CLOSEST(rdev->desc->min_uV +
				(rdev->desc->uV_step * selector), 1000);  

	low = pmbus_regulator_get_low_margin(client, rdev_get_id(rdev));
	if (low < 0)
		return low;

	high = pmbus_regulator_get_high_margin(client, rdev_get_id(rdev));
	if (high < 0)
		return high;

	if (val >= low && val <= high)
		return val * 1000;  

	return 0;
}

const struct regulator_ops pmbus_regulator_ops = {
	.enable = pmbus_regulator_enable,
	.disable = pmbus_regulator_disable,
	.is_enabled = pmbus_regulator_is_enabled,
	.get_error_flags = pmbus_regulator_get_error_flags,
	.get_status = pmbus_regulator_get_status,
	.get_voltage = pmbus_regulator_get_voltage,
	.set_voltage = pmbus_regulator_set_voltage,
	.list_voltage = pmbus_regulator_list_voltage,
};
EXPORT_SYMBOL_NS_GPL(pmbus_regulator_ops, PMBUS);

static int pmbus_regulator_register(struct pmbus_data *data)
{
	struct device *dev = data->dev;
	const struct pmbus_driver_info *info = data->info;
	const struct pmbus_platform_data *pdata = dev_get_platdata(dev);
	int i;

	data->rdevs = devm_kzalloc(dev, sizeof(struct regulator_dev *) * info->num_regulators,
				   GFP_KERNEL);
	if (!data->rdevs)
		return -ENOMEM;

	for (i = 0; i < info->num_regulators; i++) {
		struct regulator_config config = { };

		config.dev = dev;
		config.driver_data = data;

		if (pdata && pdata->reg_init_data)
			config.init_data = &pdata->reg_init_data[i];

		data->rdevs[i] = devm_regulator_register(dev, &info->reg_desc[i],
							 &config);
		if (IS_ERR(data->rdevs[i]))
			return dev_err_probe(dev, PTR_ERR(data->rdevs[i]),
					     "Failed to register %s regulator\n",
					     info->reg_desc[i].name);
	}

	return 0;
}

static int pmbus_regulator_notify(struct pmbus_data *data, int page, int event)
{
		int j;

		for (j = 0; j < data->info->num_regulators; j++) {
			if (page == rdev_get_id(data->rdevs[j])) {
				regulator_notifier_call_chain(data->rdevs[j], event, NULL);
				break;
			}
		}
		return 0;
}
#else
static int pmbus_regulator_register(struct pmbus_data *data)
{
	return 0;
}

static int pmbus_regulator_notify(struct pmbus_data *data, int page, int event)
{
		return 0;
}
#endif

static int pmbus_write_smbalert_mask(struct i2c_client *client, u8 page, u8 reg, u8 val)
{
	return pmbus_write_word_data(client, page, PMBUS_SMBALERT_MASK, reg | (val << 8));
}

static irqreturn_t pmbus_fault_handler(int irq, void *pdata)
{
	struct pmbus_data *data = pdata;
	struct i2c_client *client = to_i2c_client(data->dev);

	int i, status, event;
	mutex_lock(&data->update_lock);
	for (i = 0; i < data->info->pages; i++) {
		_pmbus_get_flags(data, i, &status, &event, true);

		if (event)
			pmbus_regulator_notify(data, i, event);
	}

	pmbus_clear_faults(client);
	mutex_unlock(&data->update_lock);

	return IRQ_HANDLED;
}

static int pmbus_irq_setup(struct i2c_client *client, struct pmbus_data *data)
{
	struct device *dev = &client->dev;
	const struct pmbus_status_category *cat;
	const struct pmbus_status_assoc *bit;
	int i, j, err, func;
	u8 mask;

	static const u8 misc_status[] = {PMBUS_STATUS_CML, PMBUS_STATUS_OTHER,
					 PMBUS_STATUS_MFR_SPECIFIC, PMBUS_STATUS_FAN_12,
					 PMBUS_STATUS_FAN_34};

	if (!client->irq)
		return 0;

	for (i = 0; i < data->info->pages; i++) {
		func = data->info->func[i];

		for (j = 0; j < ARRAY_SIZE(pmbus_status_flag_map); j++) {
			cat = &pmbus_status_flag_map[j];
			if (!(func & cat->func))
				continue;
			mask = 0;
			for (bit = cat->bits; bit->pflag; bit++)
				mask |= bit->pflag;

			err = pmbus_write_smbalert_mask(client, i, cat->reg, ~mask);
			if (err)
				dev_dbg_once(dev, "Failed to set smbalert for reg 0x%02x\n",
					     cat->reg);
		}

		for (j = 0; j < ARRAY_SIZE(misc_status); j++)
			pmbus_write_smbalert_mask(client, i, misc_status[j], 0xff);
	}

	 
	err = devm_request_threaded_irq(dev, client->irq, NULL, pmbus_fault_handler,
					IRQF_ONESHOT, "pmbus-irq", data);
	if (err) {
		dev_err(dev, "failed to request an irq %d\n", err);
		return err;
	}

	return 0;
}

static struct dentry *pmbus_debugfs_dir;	 

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int pmbus_debugfs_get(void *data, u64 *val)
{
	int rc;
	struct pmbus_debugfs_entry *entry = data;
	struct pmbus_data *pdata = i2c_get_clientdata(entry->client);

	rc = mutex_lock_interruptible(&pdata->update_lock);
	if (rc)
		return rc;
	rc = _pmbus_read_byte_data(entry->client, entry->page, entry->reg);
	mutex_unlock(&pdata->update_lock);
	if (rc < 0)
		return rc;

	*val = rc;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(pmbus_debugfs_ops, pmbus_debugfs_get, NULL,
			 "0x%02llx\n");

static int pmbus_debugfs_get_status(void *data, u64 *val)
{
	int rc;
	struct pmbus_debugfs_entry *entry = data;
	struct pmbus_data *pdata = i2c_get_clientdata(entry->client);

	rc = mutex_lock_interruptible(&pdata->update_lock);
	if (rc)
		return rc;
	rc = pdata->read_status(entry->client, entry->page);
	mutex_unlock(&pdata->update_lock);
	if (rc < 0)
		return rc;

	*val = rc;

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(pmbus_debugfs_ops_status, pmbus_debugfs_get_status,
			 NULL, "0x%04llx\n");

static ssize_t pmbus_debugfs_mfr_read(struct file *file, char __user *buf,
				       size_t count, loff_t *ppos)
{
	int rc;
	struct pmbus_debugfs_entry *entry = file->private_data;
	struct pmbus_data *pdata = i2c_get_clientdata(entry->client);
	char data[I2C_SMBUS_BLOCK_MAX + 2] = { 0 };

	rc = mutex_lock_interruptible(&pdata->update_lock);
	if (rc)
		return rc;
	rc = pmbus_read_block_data(entry->client, entry->page, entry->reg,
				   data);
	mutex_unlock(&pdata->update_lock);
	if (rc < 0)
		return rc;

	 
	data[rc] = '\n';

	 
	rc += 1;

	return simple_read_from_buffer(buf, count, ppos, data, rc);
}

static const struct file_operations pmbus_debugfs_ops_mfr = {
	.llseek = noop_llseek,
	.read = pmbus_debugfs_mfr_read,
	.write = NULL,
	.open = simple_open,
};

static void pmbus_remove_debugfs(void *data)
{
	struct dentry *entry = data;

	debugfs_remove_recursive(entry);
}

static int pmbus_init_debugfs(struct i2c_client *client,
			      struct pmbus_data *data)
{
	int i, idx = 0;
	char name[PMBUS_NAME_SIZE];
	struct pmbus_debugfs_entry *entries;

	if (!pmbus_debugfs_dir)
		return -ENODEV;

	 
	data->debugfs = debugfs_create_dir(dev_name(data->hwmon_dev),
					   pmbus_debugfs_dir);
	if (IS_ERR_OR_NULL(data->debugfs)) {
		data->debugfs = NULL;
		return -ENODEV;
	}

	 
	entries = devm_kcalloc(data->dev,
			       6 + data->info->pages * 10, sizeof(*entries),
			       GFP_KERNEL);
	if (!entries)
		return -ENOMEM;

	 
	if (pmbus_check_block_register(client, 0, PMBUS_MFR_ID)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_ID;
		debugfs_create_file("mfr_id", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	if (pmbus_check_block_register(client, 0, PMBUS_MFR_MODEL)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_MODEL;
		debugfs_create_file("mfr_model", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	if (pmbus_check_block_register(client, 0, PMBUS_MFR_REVISION)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_REVISION;
		debugfs_create_file("mfr_revision", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	if (pmbus_check_block_register(client, 0, PMBUS_MFR_LOCATION)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_LOCATION;
		debugfs_create_file("mfr_location", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	if (pmbus_check_block_register(client, 0, PMBUS_MFR_DATE)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_DATE;
		debugfs_create_file("mfr_date", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	if (pmbus_check_block_register(client, 0, PMBUS_MFR_SERIAL)) {
		entries[idx].client = client;
		entries[idx].page = 0;
		entries[idx].reg = PMBUS_MFR_SERIAL;
		debugfs_create_file("mfr_serial", 0444, data->debugfs,
				    &entries[idx++],
				    &pmbus_debugfs_ops_mfr);
	}

	 
	for (i = 0; i < data->info->pages; ++i) {
		 
		if (!i || pmbus_check_status_register(client, i)) {
			 
			entries[idx].client = client;
			entries[idx].page = i;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops_status);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_VOUT) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_VOUT;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_vout", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_IOUT) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_IOUT;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_iout", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_INPUT) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_INPUT;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_input", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_TEMP) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_TEMPERATURE;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_temp", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (pmbus_check_byte_register(client, i, PMBUS_STATUS_CML)) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_CML;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_cml", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (pmbus_check_byte_register(client, i, PMBUS_STATUS_OTHER)) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_OTHER;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_other", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (pmbus_check_byte_register(client, i,
					      PMBUS_STATUS_MFR_SPECIFIC)) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_MFR_SPECIFIC;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_mfr", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_FAN12) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_FAN_12;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_fan12", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}

		if (data->info->func[i] & PMBUS_HAVE_STATUS_FAN34) {
			entries[idx].client = client;
			entries[idx].page = i;
			entries[idx].reg = PMBUS_STATUS_FAN_34;
			scnprintf(name, PMBUS_NAME_SIZE, "status%d_fan34", i);
			debugfs_create_file(name, 0444, data->debugfs,
					    &entries[idx++],
					    &pmbus_debugfs_ops);
		}
	}

	return devm_add_action_or_reset(data->dev,
					pmbus_remove_debugfs, data->debugfs);
}
#else
static int pmbus_init_debugfs(struct i2c_client *client,
			      struct pmbus_data *data)
{
	return 0;
}
#endif	 

int pmbus_do_probe(struct i2c_client *client, struct pmbus_driver_info *info)
{
	struct device *dev = &client->dev;
	const struct pmbus_platform_data *pdata = dev_get_platdata(dev);
	struct pmbus_data *data;
	size_t groups_num = 0;
	int ret;
	int i;
	char *name;

	if (!info)
		return -ENODEV;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE
				     | I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA))
		return -ENODEV;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	if (info->groups)
		while (info->groups[groups_num])
			groups_num++;

	data->groups = devm_kcalloc(dev, groups_num + 2, sizeof(void *),
				    GFP_KERNEL);
	if (!data->groups)
		return -ENOMEM;

	i2c_set_clientdata(client, data);
	mutex_init(&data->update_lock);
	data->dev = dev;

	if (pdata)
		data->flags = pdata->flags;
	data->info = info;
	data->currpage = -1;
	data->currphase = -1;

	for (i = 0; i < ARRAY_SIZE(data->vout_low); i++) {
		data->vout_low[i] = -1;
		data->vout_high[i] = -1;
	}

	ret = pmbus_init_common(client, data, info);
	if (ret < 0)
		return ret;

	ret = pmbus_find_attributes(client, data);
	if (ret)
		return ret;

	 
	if (!data->num_attributes) {
		dev_err(dev, "No attributes found\n");
		return -ENODEV;
	}

	name = devm_kstrdup(dev, client->name, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	strreplace(name, '-', '_');

	data->groups[0] = &data->group;
	memcpy(data->groups + 1, info->groups, sizeof(void *) * groups_num);
	data->hwmon_dev = devm_hwmon_device_register_with_groups(dev,
					name, data, data->groups);
	if (IS_ERR(data->hwmon_dev)) {
		dev_err(dev, "Failed to register hwmon device\n");
		return PTR_ERR(data->hwmon_dev);
	}

	ret = pmbus_regulator_register(data);
	if (ret)
		return ret;

	ret = pmbus_irq_setup(client, data);
	if (ret)
		return ret;

	ret = pmbus_init_debugfs(client, data);
	if (ret)
		dev_warn(dev, "Failed to register debugfs\n");

	return 0;
}
EXPORT_SYMBOL_NS_GPL(pmbus_do_probe, PMBUS);

struct dentry *pmbus_get_debugfs_dir(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);

	return data->debugfs;
}
EXPORT_SYMBOL_NS_GPL(pmbus_get_debugfs_dir, PMBUS);

int pmbus_lock_interruptible(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);

	return mutex_lock_interruptible(&data->update_lock);
}
EXPORT_SYMBOL_NS_GPL(pmbus_lock_interruptible, PMBUS);

void pmbus_unlock(struct i2c_client *client)
{
	struct pmbus_data *data = i2c_get_clientdata(client);

	mutex_unlock(&data->update_lock);
}
EXPORT_SYMBOL_NS_GPL(pmbus_unlock, PMBUS);

static int __init pmbus_core_init(void)
{
	pmbus_debugfs_dir = debugfs_create_dir("pmbus", NULL);
	if (IS_ERR(pmbus_debugfs_dir))
		pmbus_debugfs_dir = NULL;

	return 0;
}

static void __exit pmbus_core_exit(void)
{
	debugfs_remove_recursive(pmbus_debugfs_dir);
}

module_init(pmbus_core_init);
module_exit(pmbus_core_exit);

MODULE_AUTHOR("Guenter Roeck");
MODULE_DESCRIPTION("PMBus core driver");
MODULE_LICENSE("GPL");
