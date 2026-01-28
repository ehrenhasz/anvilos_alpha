


#ifndef LINUX_PATA_PARPORT_H
#define LINUX_PATA_PARPORT_H

#include <linux/libata.h>

struct pi_adapter {
	struct device dev;
	struct pi_protocol *proto;	
	int port;			
	int mode;			
	int delay;			
	int unit;			
	int saved_r0;			
	int saved_r2;			
	unsigned long private;		
	struct pardevice *pardev;	
};




#define delay_p			(pi->delay ? udelay(pi->delay) : (void)0)
#define out_p(offs, byte)	do { outb(byte, pi->port + offs); delay_p; } while (0)
#define in_p(offs)		(delay_p, inb(pi->port + offs))

#define w0(byte)		out_p(0, byte)
#define r0()			in_p(0)
#define w1(byte)		out_p(1, byte)
#define r1()			in_p(1)
#define w2(byte)		out_p(2, byte)
#define r2()			in_p(2)
#define w3(byte)		out_p(3, byte)
#define w4(byte)		out_p(4, byte)
#define r4()			in_p(4)
#define w4w(data)		do { outw(data, pi->port + 4); delay_p; } while (0)
#define w4l(data)		do { outl(data, pi->port + 4); delay_p; } while (0)
#define r4w()			(delay_p, inw(pi->port + 4))
#define r4l()			(delay_p, inl(pi->port + 4))

struct pi_protocol {
	char name[8];

	int max_mode;
	int epp_first;		

	int default_delay;
	int max_units;		

	void (*write_regr)(struct pi_adapter *pi, int cont, int regr, int val);
	int (*read_regr)(struct pi_adapter *pi, int cont, int regr);
	void (*write_block)(struct pi_adapter *pi, char *buf, int count);
	void (*read_block)(struct pi_adapter *pi, char *buf, int count);

	void (*connect)(struct pi_adapter *pi);
	void (*disconnect)(struct pi_adapter *pi);

	int (*test_port)(struct pi_adapter *pi);
	int (*probe_unit)(struct pi_adapter *pi);
	int (*test_proto)(struct pi_adapter *pi);
	void (*log_adapter)(struct pi_adapter *pi);

	int (*init_proto)(struct pi_adapter *pi);
	void (*release_proto)(struct pi_adapter *pi);
	struct module *owner;
	struct device_driver driver;
	struct scsi_host_template sht;
};

#define PATA_PARPORT_SHT ATA_PIO_SHT

int pata_parport_register_driver(struct pi_protocol *pr);
void pata_parport_unregister_driver(struct pi_protocol *pr);


#define module_pata_parport_driver(__pi_protocol) \
	module_driver(__pi_protocol, pata_parport_register_driver, pata_parport_unregister_driver)

#endif 
