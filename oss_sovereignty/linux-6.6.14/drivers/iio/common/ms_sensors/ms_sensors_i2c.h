 
 

#ifndef _MS_SENSORS_I2C_H
#define _MS_SENSORS_I2C_H

#include <linux/i2c.h>
#include <linux/mutex.h>

#define MS_SENSORS_TP_PROM_WORDS_NB		8

 
struct ms_ht_dev {
	struct i2c_client *client;
	struct mutex lock;
	u8 res_index;
};

 
struct ms_tp_hw_data {
	u8 prom_len;
	u8 max_res_index;
};

 
struct ms_tp_dev {
	struct i2c_client *client;
	struct mutex lock;
	const struct ms_tp_hw_data *hw;
	u16 prom[MS_SENSORS_TP_PROM_WORDS_NB];
	u8 res_index;
};

int ms_sensors_reset(void *cli, u8 cmd, unsigned int delay);
int ms_sensors_read_prom_word(void *cli, int cmd, u16 *word);
int ms_sensors_convert_and_read(void *cli, u8 conv, u8 rd,
				unsigned int delay, u32 *adc);
int ms_sensors_read_serial(struct i2c_client *client, u64 *sn);
ssize_t ms_sensors_show_serial(struct ms_ht_dev *dev_data, char *buf);
ssize_t ms_sensors_write_resolution(struct ms_ht_dev *dev_data, u8 i);
ssize_t ms_sensors_show_battery_low(struct ms_ht_dev *dev_data, char *buf);
ssize_t ms_sensors_show_heater(struct ms_ht_dev *dev_data, char *buf);
ssize_t ms_sensors_write_heater(struct ms_ht_dev *dev_data,
				const char *buf, size_t len);
int ms_sensors_ht_read_temperature(struct ms_ht_dev *dev_data,
				   s32 *temperature);
int ms_sensors_ht_read_humidity(struct ms_ht_dev *dev_data,
				u32 *humidity);
int ms_sensors_tp_read_prom(struct ms_tp_dev *dev_data);
int ms_sensors_read_temp_and_pressure(struct ms_tp_dev *dev_data,
				      int *temperature,
				      unsigned int *pressure);

#endif  
