#ifdef CONFIG_IIO_TRIGGER
int iio_device_register_trigger_consumer(struct iio_dev *indio_dev);
void iio_device_unregister_trigger_consumer(struct iio_dev *indio_dev);
int iio_trigger_attach_poll_func(struct iio_trigger *trig,
				 struct iio_poll_func *pf);
int iio_trigger_detach_poll_func(struct iio_trigger *trig,
				 struct iio_poll_func *pf);
#else
static inline int iio_device_register_trigger_consumer(struct iio_dev *indio_dev)
{
	return 0;
}
static inline void iio_device_unregister_trigger_consumer(struct iio_dev *indio_dev)
{
}
static inline int iio_trigger_attach_poll_func(struct iio_trigger *trig,
					       struct iio_poll_func *pf)
{
	return 0;
}
static inline int iio_trigger_detach_poll_func(struct iio_trigger *trig,
					       struct iio_poll_func *pf)
{
	return 0;
}
#endif  
