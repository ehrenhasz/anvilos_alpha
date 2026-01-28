
#ifndef __DSA_LOOP_H
#define __DSA_LOOP_H

struct dsa_chip_data;

struct dsa_loop_pdata {
	
	struct dsa_chip_data cd;
	const char *name;
	unsigned int enabled_ports;
	const char *netdev;
};

#define DSA_LOOP_NUM_PORTS	6
#define DSA_LOOP_CPU_PORT	(DSA_LOOP_NUM_PORTS - 1)

#endif 
