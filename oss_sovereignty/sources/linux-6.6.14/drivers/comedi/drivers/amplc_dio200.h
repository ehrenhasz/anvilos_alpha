


#ifndef AMPLC_DIO200_H_INCLUDED
#define AMPLC_DIO200_H_INCLUDED

#include <linux/types.h>

struct comedi_device;


enum dio200_sdtype { sd_none, sd_intr, sd_8255, sd_8254, sd_timer };

#define DIO200_MAX_SUBDEVS	8
#define DIO200_MAX_ISNS		6

struct dio200_board {
	const char *name;
	unsigned char mainbar;
	unsigned short n_subdevs;	
	unsigned char sdtype[DIO200_MAX_SUBDEVS];	
	unsigned char sdinfo[DIO200_MAX_SUBDEVS];	
	unsigned int has_int_sce:1;	
	unsigned int has_clk_gat_sce:1;	
	unsigned int is_pcie:1;			
};

int amplc_dio200_common_attach(struct comedi_device *dev, unsigned int irq,
			       unsigned long req_irq_flags);


void amplc_dio200_set_enhance(struct comedi_device *dev, unsigned char val);

#endif
