


#ifndef _NI_LABPC_H
#define _NI_LABPC_H

enum transfer_type { fifo_not_empty_transfer, fifo_half_full_transfer,
	isa_dma_transfer
};

struct labpc_boardinfo {
	const char *name;
	int ai_speed;			
	unsigned ai_scan_up:1;		
	unsigned has_ao:1;		
	unsigned is_labpc1200:1;	
};

struct labpc_private {
	struct comedi_isadma *dma;
	struct comedi_8254 *counter;

	
	unsigned long long count;
	
	unsigned int cmd1;
	unsigned int cmd2;
	unsigned int cmd3;
	unsigned int cmd4;
	unsigned int cmd5;
	unsigned int cmd6;
	
	unsigned int stat1;
	unsigned int stat2;

	
	enum transfer_type current_transfer;
	
	unsigned int (*read_byte)(struct comedi_device *dev, unsigned long reg);
	void (*write_byte)(struct comedi_device *dev,
			   unsigned int byte, unsigned long reg);
};

int labpc_common_attach(struct comedi_device *dev,
			unsigned int irq, unsigned long isr_flags);
void labpc_common_detach(struct comedi_device *dev);

#endif 
