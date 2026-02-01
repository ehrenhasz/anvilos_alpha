 
 

#ifndef AMPLC_PC236_H_INCLUDED
#define AMPLC_PC236_H_INCLUDED

#include <linux/types.h>

struct comedi_device;

struct pc236_board {
	const char *name;
	void (*intr_update_cb)(struct comedi_device *dev, bool enable);
	bool (*intr_chk_clr_cb)(struct comedi_device *dev);
};

struct pc236_private {
	unsigned long lcr_iobase;  
	bool enable_irq;
};

int amplc_pc236_common_attach(struct comedi_device *dev, unsigned long iobase,
			      unsigned int irq, unsigned long req_irq_flags);

#endif
