

#ifndef __SOC_FSL_TSA_H__
#define __SOC_FSL_TSA_H__

#include <linux/types.h>

struct device_node;
struct device;
struct tsa_serial;

struct tsa_serial *tsa_serial_get_byphandle(struct device_node *np,
					    const char *phandle_name);
void tsa_serial_put(struct tsa_serial *tsa_serial);
struct tsa_serial *devm_tsa_serial_get_byphandle(struct device *dev,
						 struct device_node *np,
						 const char *phandle_name);


int tsa_serial_connect(struct tsa_serial *tsa_serial);
int tsa_serial_disconnect(struct tsa_serial *tsa_serial);


struct tsa_serial_info {
	unsigned long rx_fs_rate;
	unsigned long rx_bit_rate;
	u8 nb_rx_ts;
	unsigned long tx_fs_rate;
	unsigned long tx_bit_rate;
	u8 nb_tx_ts;
};


int tsa_serial_get_info(struct tsa_serial *tsa_serial, struct tsa_serial_info *info);

#endif 
