 
 

#ifndef PI433_H
#define PI433_H

#include <linux/types.h>
#include "rf69_enum.h"

 

enum option_on_off {
	OPTION_OFF,
	OPTION_ON
};

 

 
#define PI433_TX_CFG_IOCTL_NR	0
struct pi433_tx_cfg {
	__u32			frequency;
	__u16			bit_rate;
	__u32			dev_frequency;
	enum modulation		modulation;
	enum mod_shaping	mod_shaping;

	enum pa_ramp		pa_ramp;

	enum tx_start_condition	tx_start_condition;

	__u16			repetitions;

	 
	enum option_on_off	enable_preamble;
	enum option_on_off	enable_sync;
	enum option_on_off	enable_length_byte;
	enum option_on_off	enable_address_byte;
	enum option_on_off	enable_crc;

	__u16			preamble_length;
	__u8			sync_length;
	__u8			fixed_message_length;

	__u8			sync_pattern[8];
	__u8			address_byte;
};

 
#define PI433_RX_CFG_IOCTL_NR	1
struct pi433_rx_cfg {
	__u32			frequency;
	__u16			bit_rate;
	__u32			dev_frequency;

	enum modulation		modulation;

	__u8			rssi_threshold;
	enum threshold_decrement threshold_decrement;
	enum antenna_impedance	antenna_impedance;
	enum lna_gain		lna_gain;
	enum mantisse		bw_mantisse;	 
	__u8			bw_exponent;	 
	enum dagc		dagc;

	 
	enum option_on_off	enable_sync;

	 
	enum option_on_off	enable_length_byte;

	 
	enum address_filtering	enable_address_filtering;

	 
	enum option_on_off	enable_crc;

	__u8			sync_length;
	__u8			fixed_message_length;
	__u32			bytes_to_drop;

	__u8			sync_pattern[8];
	__u8			node_address;
	__u8			broadcast_address;
};

#define PI433_IOC_MAGIC	'r'

#define PI433_IOC_RD_TX_CFG                                             \
	_IOR(PI433_IOC_MAGIC, PI433_TX_CFG_IOCTL_NR, char[sizeof(struct pi433_tx_cfg)])
#define PI433_IOC_WR_TX_CFG                                             \
	_IOW(PI433_IOC_MAGIC, PI433_TX_CFG_IOCTL_NR, char[sizeof(struct pi433_tx_cfg)])

#define PI433_IOC_RD_RX_CFG                                             \
	_IOR(PI433_IOC_MAGIC, PI433_RX_CFG_IOCTL_NR, char[sizeof(struct pi433_rx_cfg)])
#define PI433_IOC_WR_RX_CFG                                             \
	_IOW(PI433_IOC_MAGIC, PI433_RX_CFG_IOCTL_NR, char[sizeof(struct pi433_rx_cfg)])

#endif  
