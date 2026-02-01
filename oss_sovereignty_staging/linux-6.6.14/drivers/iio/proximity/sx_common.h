 
 

#ifndef IIO_SX_COMMON_H
#define IIO_SX_COMMON_H

#include <linux/acpi.h>
#include <linux/iio/iio.h>
#include <linux/iio/types.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>

struct device;
struct i2c_client;
struct regmap_config;
struct sx_common_data;

#define SX_COMMON_REG_IRQ_SRC				0x00

#define SX_COMMON_MAX_NUM_CHANNELS	4
static_assert(SX_COMMON_MAX_NUM_CHANNELS < BITS_PER_LONG);

struct sx_common_reg_default {
	u8 reg;
	u8 def;
	const char *property;
};

 
struct sx_common_ops {
	int (*read_prox_data)(struct sx_common_data *data,
			      const struct iio_chan_spec *chan, __be16 *val);
	int (*check_whoami)(struct device *dev, struct iio_dev *indio_dev);
	int (*init_compensation)(struct iio_dev *indio_dev);
	int (*wait_for_sample)(struct sx_common_data *data);
	const struct sx_common_reg_default  *
		(*get_default_reg)(struct device *dev, int idx,
				   struct sx_common_reg_default *reg_def);
};

 
struct sx_common_chip_info {
	unsigned int reg_stat;
	unsigned int reg_irq_msk;
	unsigned int reg_enable_chan;
	unsigned int reg_reset;

	unsigned int mask_enable_chan;
	unsigned int stat_offset;
	unsigned int irq_msk_offset;
	unsigned int num_channels;
	int num_default_regs;

	struct sx_common_ops ops;

	const struct iio_chan_spec *iio_channels;
	int num_iio_channels;
	struct iio_info iio_info;
};

 
struct sx_common_data {
	const struct sx_common_chip_info *chip_info;

	struct mutex mutex;
	struct completion completion;
	struct i2c_client *client;
	struct iio_trigger *trig;
	struct regmap *regmap;

	unsigned long chan_prox_stat;
	bool trigger_enabled;

	 
	struct {
		__be16 channels[SX_COMMON_MAX_NUM_CHANNELS];
		s64 ts __aligned(8);
	} buffer;

	unsigned int suspend_ctrl;
	unsigned long chan_read;
	unsigned long chan_event;
};

int sx_common_read_proximity(struct sx_common_data *data,
			     const struct iio_chan_spec *chan, int *val);

int sx_common_read_event_config(struct iio_dev *indio_dev,
				const struct iio_chan_spec *chan,
				enum iio_event_type type,
				enum iio_event_direction dir);
int sx_common_write_event_config(struct iio_dev *indio_dev,
				 const struct iio_chan_spec *chan,
				 enum iio_event_type type,
				 enum iio_event_direction dir, int state);

int sx_common_probe(struct i2c_client *client,
		    const struct sx_common_chip_info *chip_info,
		    const struct regmap_config *regmap_config);

void sx_common_get_raw_register_config(struct device *dev,
				       struct sx_common_reg_default *reg_def);

 
extern const struct iio_event_spec sx_common_events[3];

#endif   
