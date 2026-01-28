
#ifndef __COM20020_H
#define __COM20020_H
#include <linux/leds.h>

int com20020_check(struct net_device *dev);
int com20020_found(struct net_device *dev, int shared);
extern const struct net_device_ops com20020_netdev_ops;


#define ARCNET_TOTAL_SIZE 8

#define PLX_PCI_MAX_CARDS 2

struct ledoffsets {
	int green;
	int red;
};

struct com20020_pci_channel_map {
	u32 bar;
	u32 offset;
	u32 size;               
};

struct com20020_pci_card_info {
	const char *name;
	int devcount;

	struct com20020_pci_channel_map chan_map_tbl[PLX_PCI_MAX_CARDS];
	struct com20020_pci_channel_map misc_map;

	struct ledoffsets leds[PLX_PCI_MAX_CARDS];
	int rotary;

	unsigned int flags;
};

struct com20020_priv {
	struct com20020_pci_card_info *ci;
	struct list_head list_dev;
	resource_size_t misc;
};

struct com20020_dev {
	struct list_head list;
	struct net_device *dev;

	struct led_classdev tx_led;
	struct led_classdev recon_led;

	struct com20020_priv *pci_priv;
	int index;
};

#define COM20020_REG_W_INTMASK	0	
#define COM20020_REG_R_STATUS	0	
#define COM20020_REG_W_COMMAND	1	
#define COM20020_REG_R_DIAGSTAT	1	
#define COM20020_REG_W_ADDR_HI	2	
#define COM20020_REG_W_ADDR_LO	3
#define COM20020_REG_RW_MEMDATA	4	
#define COM20020_REG_W_SUBADR	5	
#define COM20020_REG_W_CONFIG	6	
#define COM20020_REG_W_XREG	7	


#define RDDATAflag	0x80	


#define NEWNXTIDflag	0x02	


#define RESETcfg	0x80	
#define TXENcfg		0x20	
#define XTOcfg(x)	((x) << 3)	


#define PROMISCset	0x10	
#define P1MODE		0x80    
#define SLOWARB		0x01    


#define SUB_TENTATIVE	0	
#define SUB_NODE	1	
#define SUB_SETUP1	2	
#define SUB_TEST	3	


#define SUB_SETUP2	4	
#define SUB_BUSCTL	5	
#define SUB_DMACOUNT	6	

static inline void com20020_set_subaddress(struct arcnet_local *lp,
					   int ioaddr, int val)
{
	if (val < 4) {
		lp->config = (lp->config & ~0x03) | val;
		arcnet_outb(lp->config, ioaddr, COM20020_REG_W_CONFIG);
	} else {
		arcnet_outb(val, ioaddr, COM20020_REG_W_SUBADR);
	}
}

#endif 
