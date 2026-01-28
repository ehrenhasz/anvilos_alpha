#ifndef _IIO_SIMPLE_DUMMY_H_
#define _IIO_SIMPLE_DUMMY_H_
#include <linux/kernel.h>
struct iio_dummy_accel_calibscale;
struct iio_dummy_regs;
struct iio_dummy_state {
	int dac_val;
	int single_ended_adc_val;
	int differential_adc_val[2];
	int accel_val;
	int accel_calibbias;
	int activity_running;
	int activity_walking;
	const struct iio_dummy_accel_calibscale *accel_calibscale;
	struct mutex lock;
	struct iio_dummy_regs *regs;
	int steps_enabled;
	int steps;
	int height;
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
	int event_irq;
	int event_val;
	bool event_en;
	s64 event_timestamp;
#endif  
};
#ifdef CONFIG_IIO_SIMPLE_DUMMY_EVENTS
struct iio_dev;
int iio_simple_dummy_read_event_config(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       enum iio_event_type type,
				       enum iio_event_direction dir);
int iio_simple_dummy_write_event_config(struct iio_dev *indio_dev,
					const struct iio_chan_spec *chan,
					enum iio_event_type type,
					enum iio_event_direction dir,
					int state);
int iio_simple_dummy_read_event_value(struct iio_dev *indio_dev,
				      const struct iio_chan_spec *chan,
				      enum iio_event_type type,
				      enum iio_event_direction dir,
				      enum iio_event_info info, int *val,
				      int *val2);
int iio_simple_dummy_write_event_value(struct iio_dev *indio_dev,
				       const struct iio_chan_spec *chan,
				       enum iio_event_type type,
				       enum iio_event_direction dir,
				       enum iio_event_info info, int val,
				       int val2);
int iio_simple_dummy_events_register(struct iio_dev *indio_dev);
void iio_simple_dummy_events_unregister(struct iio_dev *indio_dev);
#else  
static inline int
iio_simple_dummy_events_register(struct iio_dev *indio_dev)
{
	return 0;
}
static inline void
iio_simple_dummy_events_unregister(struct iio_dev *indio_dev)
{}
#endif  
enum iio_simple_dummy_scan_elements {
	DUMMY_INDEX_VOLTAGE_0,
	DUMMY_INDEX_DIFFVOLTAGE_1M2,
	DUMMY_INDEX_DIFFVOLTAGE_3M4,
	DUMMY_INDEX_ACCELX,
};
#ifdef CONFIG_IIO_SIMPLE_DUMMY_BUFFER
int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev);
void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev);
#else
static inline int iio_simple_dummy_configure_buffer(struct iio_dev *indio_dev)
{
	return 0;
}
static inline
void iio_simple_dummy_unconfigure_buffer(struct iio_dev *indio_dev)
{}
#endif  
#endif  
