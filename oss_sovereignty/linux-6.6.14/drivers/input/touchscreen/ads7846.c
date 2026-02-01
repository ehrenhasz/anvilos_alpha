
 
#include <linux/types.h>
#include <linux/hwmon.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/property.h>
#include <linux/gpio/consumer.h>
#include <linux/spi/spi.h>
#include <linux/spi/ads7846.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <asm/unaligned.h>

 

#define TS_POLL_DELAY	1	 
#define TS_POLL_PERIOD	5	 

 
#define	SAMPLE_BITS	(8   + 16   + 2  )

struct ads7846_buf {
	u8 cmd;
	__be16 data;
} __packed;

struct ads7846_buf_layout {
	unsigned int offset;
	unsigned int count;
	unsigned int skip;
};

 
struct ads7846_packet {
	unsigned int count;
	unsigned int count_skip;
	unsigned int cmds;
	unsigned int last_cmd_idx;
	struct ads7846_buf_layout l[5];
	struct ads7846_buf *rx;
	struct ads7846_buf *tx;

	struct ads7846_buf pwrdown_cmd;

	bool ignore;
	u16 x, y, z1, z2;
};

struct ads7846 {
	struct input_dev	*input;
	char			phys[32];
	char			name[32];

	struct spi_device	*spi;
	struct regulator	*reg;

	u16			model;
	u16			vref_mv;
	u16			vref_delay_usecs;
	u16			x_plate_ohms;
	u16			pressure_max;

	bool			swap_xy;
	bool			use_internal;

	struct ads7846_packet	*packet;

	struct spi_transfer	xfer[18];
	struct spi_message	msg[5];
	int			msg_count;
	wait_queue_head_t	wait;

	bool			pendown;

	int			read_cnt;
	int			read_rep;
	int			last_read;

	u16			debounce_max;
	u16			debounce_tol;
	u16			debounce_rep;

	u16			penirq_recheck_delay_usecs;

	struct touchscreen_properties core_prop;

	struct mutex		lock;
	bool			stopped;	 
	bool			disabled;	 
	bool			suspended;	 

	int			(*filter)(void *data, int data_idx, int *val);
	void			*filter_data;
	int			(*get_pendown_state)(void);
	struct gpio_desc	*gpio_pendown;

	void			(*wait_for_sync)(void);
};

enum ads7846_filter {
	ADS7846_FILTER_OK,
	ADS7846_FILTER_REPEAT,
	ADS7846_FILTER_IGNORE,
};

 
#if	0
#define	CS_CHANGE(xfer)	((xfer).cs_change = 1)
#else
#define	CS_CHANGE(xfer)	((xfer).cs_change = 0)
#endif

 

 
#define	ADS_START		(1 << 7)
#define	ADS_A2A1A0_d_y		(1 << 4)	 
#define	ADS_A2A1A0_d_z1		(3 << 4)	 
#define	ADS_A2A1A0_d_z2		(4 << 4)	 
#define	ADS_A2A1A0_d_x		(5 << 4)	 
#define	ADS_A2A1A0_temp0	(0 << 4)	 
#define	ADS_A2A1A0_vbatt	(2 << 4)	 
#define	ADS_A2A1A0_vaux		(6 << 4)	 
#define	ADS_A2A1A0_temp1	(7 << 4)	 
#define	ADS_8_BIT		(1 << 3)
#define	ADS_12_BIT		(0 << 3)
#define	ADS_SER			(1 << 2)	 
#define	ADS_DFR			(0 << 2)	 
#define	ADS_PD10_PDOWN		(0 << 0)	 
#define	ADS_PD10_ADC_ON		(1 << 0)	 
#define	ADS_PD10_REF_ON		(2 << 0)	 
#define	ADS_PD10_ALL_ON		(3 << 0)	 

#define	MAX_12BIT	((1<<12)-1)

 
#define	READ_12BIT_DFR(x, adc, vref) (ADS_START | ADS_A2A1A0_d_ ## x \
	| ADS_12_BIT | ADS_DFR | \
	(adc ? ADS_PD10_ADC_ON : 0) | (vref ? ADS_PD10_REF_ON : 0))

#define	READ_Y(vref)	(READ_12BIT_DFR(y,  1, vref))
#define	READ_Z1(vref)	(READ_12BIT_DFR(z1, 1, vref))
#define	READ_Z2(vref)	(READ_12BIT_DFR(z2, 1, vref))
#define	READ_X(vref)	(READ_12BIT_DFR(x,  1, vref))
#define	PWRDOWN		(READ_12BIT_DFR(y,  0, 0))	 

 
#define	READ_12BIT_SER(x) (ADS_START | ADS_A2A1A0_ ## x \
	| ADS_12_BIT | ADS_SER)

#define	REF_ON	(READ_12BIT_DFR(x, 1, 1))
#define	REF_OFF	(READ_12BIT_DFR(y, 0, 0))

 
enum ads7846_cmds {
	ADS7846_X,
	ADS7846_Y,
	ADS7846_Z1,
	ADS7846_Z2,
	ADS7846_PWDOWN,
};

static int get_pendown_state(struct ads7846 *ts)
{
	if (ts->get_pendown_state)
		return ts->get_pendown_state();

	return gpiod_get_value(ts->gpio_pendown);
}

static void ads7846_report_pen_up(struct ads7846 *ts)
{
	struct input_dev *input = ts->input;

	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);

	ts->pendown = false;
	dev_vdbg(&ts->spi->dev, "UP\n");
}

 
static void ads7846_stop(struct ads7846 *ts)
{
	if (!ts->disabled && !ts->suspended) {
		 
		ts->stopped = true;
		mb();
		wake_up(&ts->wait);
		disable_irq(ts->spi->irq);
	}
}

 
static void ads7846_restart(struct ads7846 *ts)
{
	if (!ts->disabled && !ts->suspended) {
		 
		if (ts->pendown && !get_pendown_state(ts))
			ads7846_report_pen_up(ts);

		 
		ts->stopped = false;
		mb();
		enable_irq(ts->spi->irq);
	}
}

 
static void __ads7846_disable(struct ads7846 *ts)
{
	ads7846_stop(ts);
	regulator_disable(ts->reg);

	 
}

 
static void __ads7846_enable(struct ads7846 *ts)
{
	int error;

	error = regulator_enable(ts->reg);
	if (error != 0)
		dev_err(&ts->spi->dev, "Failed to enable supply: %d\n", error);

	ads7846_restart(ts);
}

static void ads7846_disable(struct ads7846 *ts)
{
	mutex_lock(&ts->lock);

	if (!ts->disabled) {

		if  (!ts->suspended)
			__ads7846_disable(ts);

		ts->disabled = true;
	}

	mutex_unlock(&ts->lock);
}

static void ads7846_enable(struct ads7846 *ts)
{
	mutex_lock(&ts->lock);

	if (ts->disabled) {

		ts->disabled = false;

		if (!ts->suspended)
			__ads7846_enable(ts);
	}

	mutex_unlock(&ts->lock);
}

 

 

struct ser_req {
	u8			ref_on;
	u8			command;
	u8			ref_off;
	u16			scratch;
	struct spi_message	msg;
	struct spi_transfer	xfer[6];
	 
	__be16 sample ____cacheline_aligned;
};

struct ads7845_ser_req {
	u8			command[3];
	struct spi_message	msg;
	struct spi_transfer	xfer[2];
	 
	u8 sample[3] ____cacheline_aligned;
};

static int ads7846_read12_ser(struct device *dev, unsigned command)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ads7846 *ts = dev_get_drvdata(dev);
	struct ser_req *req;
	int status;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	spi_message_init(&req->msg);

	 
	if (ts->use_internal) {
		req->ref_on = REF_ON;
		req->xfer[0].tx_buf = &req->ref_on;
		req->xfer[0].len = 1;
		spi_message_add_tail(&req->xfer[0], &req->msg);

		req->xfer[1].rx_buf = &req->scratch;
		req->xfer[1].len = 2;

		 
		req->xfer[1].delay.value = ts->vref_delay_usecs;
		req->xfer[1].delay.unit = SPI_DELAY_UNIT_USECS;
		spi_message_add_tail(&req->xfer[1], &req->msg);

		 
		command |= ADS_PD10_REF_ON;
	}

	 
	command |= ADS_PD10_ADC_ON;

	 
	req->command = (u8) command;
	req->xfer[2].tx_buf = &req->command;
	req->xfer[2].len = 1;
	spi_message_add_tail(&req->xfer[2], &req->msg);

	req->xfer[3].rx_buf = &req->sample;
	req->xfer[3].len = 2;
	spi_message_add_tail(&req->xfer[3], &req->msg);

	 

	 
	req->ref_off = PWRDOWN;
	req->xfer[4].tx_buf = &req->ref_off;
	req->xfer[4].len = 1;
	spi_message_add_tail(&req->xfer[4], &req->msg);

	req->xfer[5].rx_buf = &req->scratch;
	req->xfer[5].len = 2;
	CS_CHANGE(req->xfer[5]);
	spi_message_add_tail(&req->xfer[5], &req->msg);

	mutex_lock(&ts->lock);
	ads7846_stop(ts);
	status = spi_sync(spi, &req->msg);
	ads7846_restart(ts);
	mutex_unlock(&ts->lock);

	if (status == 0) {
		 
		status = be16_to_cpu(req->sample);
		status = status >> 3;
		status &= 0x0fff;
	}

	kfree(req);
	return status;
}

static int ads7845_read12_ser(struct device *dev, unsigned command)
{
	struct spi_device *spi = to_spi_device(dev);
	struct ads7846 *ts = dev_get_drvdata(dev);
	struct ads7845_ser_req *req;
	int status;

	req = kzalloc(sizeof *req, GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	spi_message_init(&req->msg);

	req->command[0] = (u8) command;
	req->xfer[0].tx_buf = req->command;
	req->xfer[0].rx_buf = req->sample;
	req->xfer[0].len = 3;
	spi_message_add_tail(&req->xfer[0], &req->msg);

	mutex_lock(&ts->lock);
	ads7846_stop(ts);
	status = spi_sync(spi, &req->msg);
	ads7846_restart(ts);
	mutex_unlock(&ts->lock);

	if (status == 0) {
		 
		status = get_unaligned_be16(&req->sample[1]);
		status = status >> 3;
		status &= 0x0fff;
	}

	kfree(req);
	return status;
}

#if IS_ENABLED(CONFIG_HWMON)

#define SHOW(name, var, adjust) static ssize_t \
name ## _show(struct device *dev, struct device_attribute *attr, char *buf) \
{ \
	struct ads7846 *ts = dev_get_drvdata(dev); \
	ssize_t v = ads7846_read12_ser(&ts->spi->dev, \
			READ_12BIT_SER(var)); \
	if (v < 0) \
		return v; \
	return sprintf(buf, "%u\n", adjust(ts, v)); \
} \
static DEVICE_ATTR(name, S_IRUGO, name ## _show, NULL);


 
static inline unsigned null_adjust(struct ads7846 *ts, ssize_t v)
{
	return v;
}

SHOW(temp0, temp0, null_adjust)		 
SHOW(temp1, temp1, null_adjust)		 


 
static inline unsigned vaux_adjust(struct ads7846 *ts, ssize_t v)
{
	unsigned retval = v;

	 
	retval *= ts->vref_mv;
	retval = retval >> 12;

	return retval;
}

static inline unsigned vbatt_adjust(struct ads7846 *ts, ssize_t v)
{
	unsigned retval = vaux_adjust(ts, v);

	 
	if (ts->model == 7846)
		retval *= 4;

	return retval;
}

SHOW(in0_input, vaux, vaux_adjust)
SHOW(in1_input, vbatt, vbatt_adjust)

static umode_t ads7846_is_visible(struct kobject *kobj, struct attribute *attr,
				  int index)
{
	struct device *dev = kobj_to_dev(kobj);
	struct ads7846 *ts = dev_get_drvdata(dev);

	if (ts->model == 7843 && index < 2)	 
		return 0;
	if (ts->model == 7845 && index != 2)	 
		return 0;

	return attr->mode;
}

static struct attribute *ads7846_attributes[] = {
	&dev_attr_temp0.attr,		 
	&dev_attr_temp1.attr,		 
	&dev_attr_in0_input.attr,	 
	&dev_attr_in1_input.attr,	 
	NULL,
};

static const struct attribute_group ads7846_attr_group = {
	.attrs = ads7846_attributes,
	.is_visible = ads7846_is_visible,
};
__ATTRIBUTE_GROUPS(ads7846_attr);

static int ads784x_hwmon_register(struct spi_device *spi, struct ads7846 *ts)
{
	struct device *hwmon;

	 
	switch (ts->model) {
	case 7846:
		if (!ts->vref_mv) {
			dev_dbg(&spi->dev, "assuming 2.5V internal vREF\n");
			ts->vref_mv = 2500;
			ts->use_internal = true;
		}
		break;
	case 7845:
	case 7843:
		if (!ts->vref_mv) {
			dev_warn(&spi->dev,
				"external vREF for ADS%d not specified\n",
				ts->model);
			return 0;
		}
		break;
	}

	hwmon = devm_hwmon_device_register_with_groups(&spi->dev,
						       spi->modalias, ts,
						       ads7846_attr_groups);

	return PTR_ERR_OR_ZERO(hwmon);
}

#else
static inline int ads784x_hwmon_register(struct spi_device *spi,
					 struct ads7846 *ts)
{
	return 0;
}
#endif

static ssize_t ads7846_pen_down_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->pendown);
}

static DEVICE_ATTR(pen_down, S_IRUGO, ads7846_pen_down_show, NULL);

static ssize_t ads7846_disable_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", ts->disabled);
}

static ssize_t ads7846_disable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct ads7846 *ts = dev_get_drvdata(dev);
	unsigned int i;
	int err;

	err = kstrtouint(buf, 10, &i);
	if (err)
		return err;

	if (i)
		ads7846_disable(ts);
	else
		ads7846_enable(ts);

	return count;
}

static DEVICE_ATTR(disable, 0664, ads7846_disable_show, ads7846_disable_store);

static struct attribute *ads784x_attributes[] = {
	&dev_attr_pen_down.attr,
	&dev_attr_disable.attr,
	NULL,
};

static const struct attribute_group ads784x_attr_group = {
	.attrs = ads784x_attributes,
};

 

static void null_wait_for_sync(void)
{
}

static int ads7846_debounce_filter(void *ads, int data_idx, int *val)
{
	struct ads7846 *ts = ads;

	if (!ts->read_cnt || (abs(ts->last_read - *val) > ts->debounce_tol)) {
		 
		ts->read_rep = 0;
		 
		if (ts->read_cnt < ts->debounce_max) {
			ts->last_read = *val;
			ts->read_cnt++;
			return ADS7846_FILTER_REPEAT;
		} else {
			 
			ts->read_cnt = 0;
			return ADS7846_FILTER_IGNORE;
		}
	} else {
		if (++ts->read_rep > ts->debounce_rep) {
			 
			ts->read_cnt = 0;
			ts->read_rep = 0;
			return ADS7846_FILTER_OK;
		} else {
			 
			ts->read_cnt++;
			return ADS7846_FILTER_REPEAT;
		}
	}
}

static int ads7846_no_filter(void *ads, int data_idx, int *val)
{
	return ADS7846_FILTER_OK;
}

static int ads7846_get_value(struct ads7846_buf *buf)
{
	int value;

	value = be16_to_cpup(&buf->data);

	 
	return (value >> 3) & 0xfff;
}

static void ads7846_set_cmd_val(struct ads7846 *ts, enum ads7846_cmds cmd_idx,
				u16 val)
{
	struct ads7846_packet *packet = ts->packet;

	switch (cmd_idx) {
	case ADS7846_Y:
		packet->y = val;
		break;
	case ADS7846_X:
		packet->x = val;
		break;
	case ADS7846_Z1:
		packet->z1 = val;
		break;
	case ADS7846_Z2:
		packet->z2 = val;
		break;
	default:
		WARN_ON_ONCE(1);
	}
}

static u8 ads7846_get_cmd(enum ads7846_cmds cmd_idx, int vref)
{
	switch (cmd_idx) {
	case ADS7846_Y:
		return READ_Y(vref);
	case ADS7846_X:
		return READ_X(vref);

	 
	case ADS7846_Z1:
		return READ_Z1(vref);
	case ADS7846_Z2:
		return READ_Z2(vref);
	case ADS7846_PWDOWN:
		return PWRDOWN;
	default:
		WARN_ON_ONCE(1);
	}

	return 0;
}

static bool ads7846_cmd_need_settle(enum ads7846_cmds cmd_idx)
{
	switch (cmd_idx) {
	case ADS7846_X:
	case ADS7846_Y:
	case ADS7846_Z1:
	case ADS7846_Z2:
		return true;
	case ADS7846_PWDOWN:
		return false;
	default:
		WARN_ON_ONCE(1);
	}

	return false;
}

static int ads7846_filter(struct ads7846 *ts)
{
	struct ads7846_packet *packet = ts->packet;
	int action;
	int val;
	unsigned int cmd_idx, b;

	packet->ignore = false;
	for (cmd_idx = packet->last_cmd_idx; cmd_idx < packet->cmds - 1; cmd_idx++) {
		struct ads7846_buf_layout *l = &packet->l[cmd_idx];

		packet->last_cmd_idx = cmd_idx;

		for (b = l->skip; b < l->count; b++) {
			val = ads7846_get_value(&packet->rx[l->offset + b]);

			action = ts->filter(ts->filter_data, cmd_idx, &val);
			if (action == ADS7846_FILTER_REPEAT) {
				if (b == l->count - 1)
					return -EAGAIN;
			} else if (action == ADS7846_FILTER_OK) {
				ads7846_set_cmd_val(ts, cmd_idx, val);
				break;
			} else {
				packet->ignore = true;
				return 0;
			}
		}
	}

	return 0;
}

static void ads7846_read_state(struct ads7846 *ts)
{
	struct ads7846_packet *packet = ts->packet;
	struct spi_message *m;
	int msg_idx = 0;
	int error;

	packet->last_cmd_idx = 0;

	while (true) {
		ts->wait_for_sync();

		m = &ts->msg[msg_idx];
		error = spi_sync(ts->spi, m);
		if (error) {
			dev_err(&ts->spi->dev, "spi_sync --> %d\n", error);
			packet->ignore = true;
			return;
		}

		error = ads7846_filter(ts);
		if (error)
			continue;

		return;
	}
}

static void ads7846_report_state(struct ads7846 *ts)
{
	struct ads7846_packet *packet = ts->packet;
	unsigned int Rt;
	u16 x, y, z1, z2;

	x = packet->x;
	y = packet->y;
	if (ts->model == 7845) {
		z1 = 0;
		z2 = 0;
	} else {
		z1 = packet->z1;
		z2 = packet->z2;
	}

	 
	if (x == MAX_12BIT)
		x = 0;

	if (ts->model == 7843 || ts->model == 7845) {
		Rt = ts->pressure_max / 2;
	} else if (likely(x && z1)) {
		 
		Rt = z2;
		Rt -= z1;
		Rt *= ts->x_plate_ohms;
		Rt = DIV_ROUND_CLOSEST(Rt, 16);
		Rt *= x;
		Rt /= z1;
		Rt = DIV_ROUND_CLOSEST(Rt, 256);
	} else {
		Rt = 0;
	}

	 
	if (packet->ignore || Rt > ts->pressure_max) {
		dev_vdbg(&ts->spi->dev, "ignored %d pressure %d\n",
			 packet->ignore, Rt);
		return;
	}

	 
	if (ts->penirq_recheck_delay_usecs) {
		udelay(ts->penirq_recheck_delay_usecs);
		if (!get_pendown_state(ts))
			Rt = 0;
	}

	 
	if (Rt) {
		struct input_dev *input = ts->input;

		if (!ts->pendown) {
			input_report_key(input, BTN_TOUCH, 1);
			ts->pendown = true;
			dev_vdbg(&ts->spi->dev, "DOWN\n");
		}

		touchscreen_report_pos(input, &ts->core_prop, x, y, false);
		input_report_abs(input, ABS_PRESSURE, ts->pressure_max - Rt);

		input_sync(input);
		dev_vdbg(&ts->spi->dev, "%4d/%4d/%4d\n", x, y, Rt);
	}
}

static irqreturn_t ads7846_hard_irq(int irq, void *handle)
{
	struct ads7846 *ts = handle;

	return get_pendown_state(ts) ? IRQ_WAKE_THREAD : IRQ_HANDLED;
}


static irqreturn_t ads7846_irq(int irq, void *handle)
{
	struct ads7846 *ts = handle;

	 
	msleep(TS_POLL_DELAY);

	while (!ts->stopped && get_pendown_state(ts)) {

		 
		ads7846_read_state(ts);

		if (!ts->stopped)
			ads7846_report_state(ts);

		wait_event_timeout(ts->wait, ts->stopped,
				   msecs_to_jiffies(TS_POLL_PERIOD));
	}

	if (ts->pendown && !ts->stopped)
		ads7846_report_pen_up(ts);

	return IRQ_HANDLED;
}

static int ads7846_suspend(struct device *dev)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->lock);

	if (!ts->suspended) {

		if (!ts->disabled)
			__ads7846_disable(ts);

		if (device_may_wakeup(&ts->spi->dev))
			enable_irq_wake(ts->spi->irq);

		ts->suspended = true;
	}

	mutex_unlock(&ts->lock);

	return 0;
}

static int ads7846_resume(struct device *dev)
{
	struct ads7846 *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->lock);

	if (ts->suspended) {

		ts->suspended = false;

		if (device_may_wakeup(&ts->spi->dev))
			disable_irq_wake(ts->spi->irq);

		if (!ts->disabled)
			__ads7846_enable(ts);
	}

	mutex_unlock(&ts->lock);

	return 0;
}

static DEFINE_SIMPLE_DEV_PM_OPS(ads7846_pm, ads7846_suspend, ads7846_resume);

static int ads7846_setup_pendown(struct spi_device *spi,
				 struct ads7846 *ts,
				 const struct ads7846_platform_data *pdata)
{
	 

	if (pdata->get_pendown_state) {
		ts->get_pendown_state = pdata->get_pendown_state;
	} else {
		ts->gpio_pendown = gpiod_get(&spi->dev, "pendown", GPIOD_IN);
		if (IS_ERR(ts->gpio_pendown)) {
			dev_err(&spi->dev, "failed to request pendown GPIO\n");
			return PTR_ERR(ts->gpio_pendown);
		}
		if (pdata->gpio_pendown_debounce)
			gpiod_set_debounce(ts->gpio_pendown,
					   pdata->gpio_pendown_debounce);
	}

	return 0;
}

 
static int ads7846_setup_spi_msg(struct ads7846 *ts,
				  const struct ads7846_platform_data *pdata)
{
	struct spi_message *m = &ts->msg[0];
	struct spi_transfer *x = ts->xfer;
	struct ads7846_packet *packet = ts->packet;
	int vref = pdata->keep_vref_on;
	unsigned int count, offset = 0;
	unsigned int cmd_idx, b;
	unsigned long time;
	size_t size = 0;

	 
	time = NSEC_PER_SEC / ts->spi->max_speed_hz;

	count = pdata->settle_delay_usecs * NSEC_PER_USEC / time;
	packet->count_skip = DIV_ROUND_UP(count, 24);

	if (ts->debounce_max && ts->debounce_rep)
		 
		packet->count = ts->debounce_rep + 2;
	else
		packet->count = 1;

	if (ts->model == 7846)
		packet->cmds = 5;  
	else
		packet->cmds = 3;  

	for (cmd_idx = 0; cmd_idx < packet->cmds; cmd_idx++) {
		struct ads7846_buf_layout *l = &packet->l[cmd_idx];
		unsigned int max_count;

		if (cmd_idx == packet->cmds - 1)
			cmd_idx = ADS7846_PWDOWN;

		if (ads7846_cmd_need_settle(cmd_idx))
			max_count = packet->count + packet->count_skip;
		else
			max_count = packet->count;

		l->offset = offset;
		offset += max_count;
		l->count = max_count;
		l->skip = packet->count_skip;
		size += sizeof(*packet->tx) * max_count;
	}

	packet->tx = devm_kzalloc(&ts->spi->dev, size, GFP_KERNEL);
	if (!packet->tx)
		return -ENOMEM;

	packet->rx = devm_kzalloc(&ts->spi->dev, size, GFP_KERNEL);
	if (!packet->rx)
		return -ENOMEM;

	if (ts->model == 7873) {
		 
		ts->model = 7846;
		vref = 0;
	}

	ts->msg_count = 1;
	spi_message_init(m);
	m->context = ts;

	for (cmd_idx = 0; cmd_idx < packet->cmds; cmd_idx++) {
		struct ads7846_buf_layout *l = &packet->l[cmd_idx];
		u8 cmd;

		if (cmd_idx == packet->cmds - 1)
			cmd_idx = ADS7846_PWDOWN;

		cmd = ads7846_get_cmd(cmd_idx, vref);

		for (b = 0; b < l->count; b++)
			packet->tx[l->offset + b].cmd = cmd;
	}

	x->tx_buf = packet->tx;
	x->rx_buf = packet->rx;
	x->len = size;
	spi_message_add_tail(x, m);

	return 0;
}

static const struct of_device_id ads7846_dt_ids[] = {
	{ .compatible = "ti,tsc2046",	.data = (void *) 7846 },
	{ .compatible = "ti,ads7843",	.data = (void *) 7843 },
	{ .compatible = "ti,ads7845",	.data = (void *) 7845 },
	{ .compatible = "ti,ads7846",	.data = (void *) 7846 },
	{ .compatible = "ti,ads7873",	.data = (void *) 7873 },
	{ }
};
MODULE_DEVICE_TABLE(of, ads7846_dt_ids);

static const struct ads7846_platform_data *ads7846_get_props(struct device *dev)
{
	struct ads7846_platform_data *pdata;
	u32 value;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->model = (uintptr_t)device_get_match_data(dev);

	device_property_read_u16(dev, "ti,vref-delay-usecs",
				 &pdata->vref_delay_usecs);
	device_property_read_u16(dev, "ti,vref-mv", &pdata->vref_mv);
	pdata->keep_vref_on = device_property_read_bool(dev, "ti,keep-vref-on");

	pdata->swap_xy = device_property_read_bool(dev, "ti,swap-xy");

	device_property_read_u16(dev, "ti,settle-delay-usec",
				 &pdata->settle_delay_usecs);
	device_property_read_u16(dev, "ti,penirq-recheck-delay-usecs",
				 &pdata->penirq_recheck_delay_usecs);

	device_property_read_u16(dev, "ti,x-plate-ohms", &pdata->x_plate_ohms);
	device_property_read_u16(dev, "ti,y-plate-ohms", &pdata->y_plate_ohms);

	device_property_read_u16(dev, "ti,x-min", &pdata->x_min);
	device_property_read_u16(dev, "ti,y-min", &pdata->y_min);
	device_property_read_u16(dev, "ti,x-max", &pdata->x_max);
	device_property_read_u16(dev, "ti,y-max", &pdata->y_max);

	 
	device_property_read_u16(dev, "ti,pressure-min", &pdata->pressure_min);
	if (!device_property_read_u32(dev, "touchscreen-min-pressure", &value))
		pdata->pressure_min = (u16) value;
	device_property_read_u16(dev, "ti,pressure-max", &pdata->pressure_max);

	device_property_read_u16(dev, "ti,debounce-max", &pdata->debounce_max);
	if (!device_property_read_u32(dev, "touchscreen-average-samples", &value))
		pdata->debounce_max = (u16) value;
	device_property_read_u16(dev, "ti,debounce-tol", &pdata->debounce_tol);
	device_property_read_u16(dev, "ti,debounce-rep", &pdata->debounce_rep);

	device_property_read_u32(dev, "ti,pendown-gpio-debounce",
			     &pdata->gpio_pendown_debounce);

	pdata->wakeup = device_property_read_bool(dev, "wakeup-source") ||
			device_property_read_bool(dev, "linux,wakeup");

	return pdata;
}

static void ads7846_regulator_disable(void *regulator)
{
	regulator_disable(regulator);
}

static int ads7846_probe(struct spi_device *spi)
{
	const struct ads7846_platform_data *pdata;
	struct ads7846 *ts;
	struct device *dev = &spi->dev;
	struct ads7846_packet *packet;
	struct input_dev *input_dev;
	unsigned long irq_flags;
	int err;

	if (!spi->irq) {
		dev_dbg(dev, "no IRQ?\n");
		return -EINVAL;
	}

	 
	if (spi->max_speed_hz > (125000 * SAMPLE_BITS)) {
		dev_err(dev, "f(sample) %d KHz?\n",
			(spi->max_speed_hz/SAMPLE_BITS)/1000);
		return -EINVAL;
	}

	 
	spi->bits_per_word = 8;
	spi->mode &= ~SPI_MODE_X_MASK;
	spi->mode |= SPI_MODE_0;
	err = spi_setup(spi);
	if (err < 0)
		return err;

	ts = devm_kzalloc(dev, sizeof(struct ads7846), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	packet = devm_kzalloc(dev, sizeof(struct ads7846_packet), GFP_KERNEL);
	if (!packet)
		return -ENOMEM;

	input_dev = devm_input_allocate_device(dev);
	if (!input_dev)
		return -ENOMEM;

	spi_set_drvdata(spi, ts);

	ts->packet = packet;
	ts->spi = spi;
	ts->input = input_dev;

	mutex_init(&ts->lock);
	init_waitqueue_head(&ts->wait);

	pdata = dev_get_platdata(dev);
	if (!pdata) {
		pdata = ads7846_get_props(dev);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	ts->model = pdata->model ? : 7846;
	ts->vref_delay_usecs = pdata->vref_delay_usecs ? : 100;
	ts->x_plate_ohms = pdata->x_plate_ohms ? : 400;
	ts->vref_mv = pdata->vref_mv;

	if (pdata->debounce_max) {
		ts->debounce_max = pdata->debounce_max;
		if (ts->debounce_max < 2)
			ts->debounce_max = 2;
		ts->debounce_tol = pdata->debounce_tol;
		ts->debounce_rep = pdata->debounce_rep;
		ts->filter = ads7846_debounce_filter;
		ts->filter_data = ts;
	} else {
		ts->filter = ads7846_no_filter;
	}

	err = ads7846_setup_pendown(spi, ts, pdata);
	if (err)
		return err;

	if (pdata->penirq_recheck_delay_usecs)
		ts->penirq_recheck_delay_usecs =
				pdata->penirq_recheck_delay_usecs;

	ts->wait_for_sync = pdata->wait_for_sync ? : null_wait_for_sync;

	snprintf(ts->phys, sizeof(ts->phys), "%s/input0", dev_name(dev));
	snprintf(ts->name, sizeof(ts->name), "ADS%d Touchscreen", ts->model);

	input_dev->name = ts->name;
	input_dev->phys = ts->phys;

	input_dev->id.bustype = BUS_SPI;
	input_dev->id.product = pdata->model;

	input_set_capability(input_dev, EV_KEY, BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X,
			pdata->x_min ? : 0,
			pdata->x_max ? : MAX_12BIT,
			0, 0);
	input_set_abs_params(input_dev, ABS_Y,
			pdata->y_min ? : 0,
			pdata->y_max ? : MAX_12BIT,
			0, 0);
	if (ts->model != 7845)
		input_set_abs_params(input_dev, ABS_PRESSURE,
				pdata->pressure_min, pdata->pressure_max, 0, 0);

	 
	touchscreen_parse_properties(ts->input, false, &ts->core_prop);
	ts->pressure_max = input_abs_get_max(input_dev, ABS_PRESSURE) ? : ~0;

	 
	if (!ts->core_prop.swap_x_y && pdata->swap_xy) {
		swap(input_dev->absinfo[ABS_X], input_dev->absinfo[ABS_Y]);
		ts->core_prop.swap_x_y = true;
	}

	ads7846_setup_spi_msg(ts, pdata);

	ts->reg = devm_regulator_get(dev, "vcc");
	if (IS_ERR(ts->reg)) {
		err = PTR_ERR(ts->reg);
		dev_err(dev, "unable to get regulator: %d\n", err);
		return err;
	}

	err = regulator_enable(ts->reg);
	if (err) {
		dev_err(dev, "unable to enable regulator: %d\n", err);
		return err;
	}

	err = devm_add_action_or_reset(dev, ads7846_regulator_disable, ts->reg);
	if (err)
		return err;

	irq_flags = pdata->irq_flags ? : IRQF_TRIGGER_FALLING;
	irq_flags |= IRQF_ONESHOT;

	err = devm_request_threaded_irq(dev, spi->irq,
					ads7846_hard_irq, ads7846_irq,
					irq_flags, dev->driver->name, ts);
	if (err && err != -EPROBE_DEFER && !pdata->irq_flags) {
		dev_info(dev,
			"trying pin change workaround on irq %d\n", spi->irq);
		irq_flags |= IRQF_TRIGGER_RISING;
		err = devm_request_threaded_irq(dev, spi->irq,
						ads7846_hard_irq, ads7846_irq,
						irq_flags, dev->driver->name,
						ts);
	}

	if (err) {
		dev_dbg(dev, "irq %d busy?\n", spi->irq);
		return err;
	}

	err = ads784x_hwmon_register(spi, ts);
	if (err)
		return err;

	dev_info(dev, "touchscreen, irq %d\n", spi->irq);

	 
	if (ts->model == 7845)
		ads7845_read12_ser(dev, PWRDOWN);
	else
		(void) ads7846_read12_ser(dev, READ_12BIT_SER(vaux));

	err = devm_device_add_group(dev, &ads784x_attr_group);
	if (err)
		return err;

	err = input_register_device(input_dev);
	if (err)
		return err;

	device_init_wakeup(dev, pdata->wakeup);

	 
	if (!dev_get_platdata(dev))
		devm_kfree(dev, (void *)pdata);

	return 0;
}

static void ads7846_remove(struct spi_device *spi)
{
	struct ads7846 *ts = spi_get_drvdata(spi);

	ads7846_stop(ts);
}

static struct spi_driver ads7846_driver = {
	.driver = {
		.name	= "ads7846",
		.pm	= pm_sleep_ptr(&ads7846_pm),
		.of_match_table = ads7846_dt_ids,
	},
	.probe		= ads7846_probe,
	.remove		= ads7846_remove,
};

module_spi_driver(ads7846_driver);

MODULE_DESCRIPTION("ADS7846 TouchScreen Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:ads7846");
