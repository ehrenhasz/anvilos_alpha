 
 
#ifndef __ADI_AXI_ADC_H__
#define __ADI_AXI_ADC_H__

struct device;
struct iio_chan_spec;

 
struct adi_axi_adc_chip_info {
	const char			*name;
	unsigned int			id;

	const struct iio_chan_spec	*channels;
	unsigned int			num_channels;

	const unsigned int		(*scale_table)[2];
	int				num_scales;

	unsigned long			max_rate;
};

 
struct adi_axi_adc_conv {
	const struct adi_axi_adc_chip_info		*chip_info;

	int (*preenable_setup)(struct adi_axi_adc_conv *conv);
	int (*reg_access)(struct adi_axi_adc_conv *conv, unsigned int reg,
			  unsigned int writeval, unsigned int *readval);
	int (*read_raw)(struct adi_axi_adc_conv *conv,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask);
	int (*write_raw)(struct adi_axi_adc_conv *conv,
			 struct iio_chan_spec const *chan,
			 int val, int val2, long mask);
	int (*read_avail)(struct adi_axi_adc_conv *conv,
			  struct iio_chan_spec const *chan,
			  const int **val, int *type, int *length, long mask);
};

struct adi_axi_adc_conv *devm_adi_axi_adc_conv_register(struct device *dev,
							size_t sizeof_priv);

void *adi_axi_adc_conv_priv(struct adi_axi_adc_conv *conv);

#endif
