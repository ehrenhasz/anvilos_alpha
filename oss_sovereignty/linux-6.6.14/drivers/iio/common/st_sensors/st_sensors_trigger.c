
 

#include <linux/kernel.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/iio/common/st_sensors.h>
#include "st_sensors_core.h"

 
static bool st_sensors_new_samples_available(struct iio_dev *indio_dev,
					     struct st_sensor_data *sdata)
{
	int ret, status;

	 
	if (!sdata->sensor_settings->drdy_irq.stat_drdy.addr)
		return true;

	 
	if (!indio_dev->active_scan_mask)
		return false;

	ret = regmap_read(sdata->regmap,
			  sdata->sensor_settings->drdy_irq.stat_drdy.addr,
			  &status);
	if (ret < 0) {
		dev_err(indio_dev->dev.parent,
			"error checking samples available\n");
		return false;
	}

	return !!(status & sdata->sensor_settings->drdy_irq.stat_drdy.mask);
}

 
static irqreturn_t st_sensors_irq_handler(int irq, void *p)
{
	struct iio_trigger *trig = p;
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct st_sensor_data *sdata = iio_priv(indio_dev);

	 
	sdata->hw_timestamp = iio_get_time_ns(indio_dev);
	return IRQ_WAKE_THREAD;
}

 
static irqreturn_t st_sensors_irq_thread(int irq, void *p)
{
	struct iio_trigger *trig = p;
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);
	struct st_sensor_data *sdata = iio_priv(indio_dev);

	 
	if (sdata->hw_irq_trigger &&
	    st_sensors_new_samples_available(indio_dev, sdata)) {
		iio_trigger_poll_nested(p);
	} else {
		dev_dbg(indio_dev->dev.parent, "spurious IRQ\n");
		return IRQ_NONE;
	}

	 
	if (!sdata->edge_irq)
		return IRQ_HANDLED;

	 
	while (sdata->hw_irq_trigger &&
	       st_sensors_new_samples_available(indio_dev, sdata)) {
		dev_dbg(indio_dev->dev.parent,
			"more samples came in during polling\n");
		sdata->hw_timestamp = iio_get_time_ns(indio_dev);
		iio_trigger_poll_nested(p);
	}

	return IRQ_HANDLED;
}

int st_sensors_allocate_trigger(struct iio_dev *indio_dev,
				const struct iio_trigger_ops *trigger_ops)
{
	struct st_sensor_data *sdata = iio_priv(indio_dev);
	struct device *parent = indio_dev->dev.parent;
	unsigned long irq_trig;
	int err;

	sdata->trig = devm_iio_trigger_alloc(parent, "%s-trigger",
					     indio_dev->name);
	if (sdata->trig == NULL) {
		dev_err(&indio_dev->dev, "failed to allocate iio trigger.\n");
		return -ENOMEM;
	}

	iio_trigger_set_drvdata(sdata->trig, indio_dev);
	sdata->trig->ops = trigger_ops;

	irq_trig = irqd_get_trigger_type(irq_get_irq_data(sdata->irq));
	 
	switch(irq_trig) {
	case IRQF_TRIGGER_FALLING:
	case IRQF_TRIGGER_LOW:
		if (!sdata->sensor_settings->drdy_irq.addr_ihl) {
			dev_err(&indio_dev->dev,
				"falling/low specified for IRQ but hardware supports only rising/high: will request rising/high\n");
			if (irq_trig == IRQF_TRIGGER_FALLING)
				irq_trig = IRQF_TRIGGER_RISING;
			if (irq_trig == IRQF_TRIGGER_LOW)
				irq_trig = IRQF_TRIGGER_HIGH;
		} else {
			 
			err = st_sensors_write_data_with_mask(indio_dev,
				sdata->sensor_settings->drdy_irq.addr_ihl,
				sdata->sensor_settings->drdy_irq.mask_ihl, 1);
			if (err < 0)
				return err;
			dev_info(&indio_dev->dev,
				 "interrupts on the falling edge or active low level\n");
		}
		break;
	case IRQF_TRIGGER_RISING:
		dev_info(&indio_dev->dev,
			 "interrupts on the rising edge\n");
		break;
	case IRQF_TRIGGER_HIGH:
		dev_info(&indio_dev->dev,
			 "interrupts active high level\n");
		break;
	default:
		 
		dev_err(&indio_dev->dev,
			"unsupported IRQ trigger specified (%lx), enforce rising edge\n", irq_trig);
		irq_trig = IRQF_TRIGGER_RISING;
	}

	 
	if (irq_trig == IRQF_TRIGGER_FALLING ||
	    irq_trig == IRQF_TRIGGER_RISING) {
		if (!sdata->sensor_settings->drdy_irq.stat_drdy.addr) {
			dev_err(&indio_dev->dev,
				"edge IRQ not supported w/o stat register.\n");
			return -EOPNOTSUPP;
		}
		sdata->edge_irq = true;
	} else {
		 
		irq_trig |= IRQF_ONESHOT;
	}

	 
	if (sdata->int_pin_open_drain &&
	    sdata->sensor_settings->drdy_irq.stat_drdy.addr)
		irq_trig |= IRQF_SHARED;

	err = devm_request_threaded_irq(parent,
					sdata->irq,
					st_sensors_irq_handler,
					st_sensors_irq_thread,
					irq_trig,
					sdata->trig->name,
					sdata->trig);
	if (err) {
		dev_err(&indio_dev->dev, "failed to request trigger IRQ.\n");
		return err;
	}

	err = devm_iio_trigger_register(parent, sdata->trig);
	if (err < 0) {
		dev_err(&indio_dev->dev, "failed to register iio trigger.\n");
		return err;
	}
	indio_dev->trig = iio_trigger_get(sdata->trig);

	return 0;
}
EXPORT_SYMBOL_NS(st_sensors_allocate_trigger, IIO_ST_SENSORS);

int st_sensors_validate_device(struct iio_trigger *trig,
			       struct iio_dev *indio_dev)
{
	struct iio_dev *indio = iio_trigger_get_drvdata(trig);

	if (indio != indio_dev)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_NS(st_sensors_validate_device, IIO_ST_SENSORS);
