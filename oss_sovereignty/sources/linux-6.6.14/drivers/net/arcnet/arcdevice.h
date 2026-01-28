

#ifndef _LINUX_ARCDEVICE_H
#define _LINUX_ARCDEVICE_H

#include <asm/timex.h>
#include <linux/if_arcnet.h>

#ifdef __KERNEL__
#include <linux/interrupt.h>


#define RECON_THRESHOLD 30


#define TX_TIMEOUT (HZ * 200 / 1000)


#undef ALPHA_WARNING


#define D_NORMAL	1	
#define D_EXTRA		2	
#define	D_INIT		4	
#define D_INIT_REASONS	8	
#define D_RECON		32	
#define D_PROTO		64	

#define D_DURING	128	
#define D_TX	        256	
#define D_RX		512	
#define D_SKB		1024	
#define D_SKB_SIZE	2048	
#define D_TIMING	4096	
#define D_DEBUG         8192    

#ifndef ARCNET_DEBUG_MAX
#define ARCNET_DEBUG_MAX (127)	
#endif

#ifndef ARCNET_DEBUG
#define ARCNET_DEBUG (D_NORMAL | D_EXTRA)
#endif
extern int arcnet_debug;

#define BUGLVL(x)	((x) & ARCNET_DEBUG_MAX & arcnet_debug)


#define arc_printk(x, dev, fmt, ...)					\
do {									\
	if (BUGLVL(x)) {						\
		if ((x) == D_NORMAL)					\
			netdev_warn(dev, fmt, ##__VA_ARGS__);		\
		else if ((x) < D_DURING)				\
			netdev_info(dev, fmt, ##__VA_ARGS__);		\
		else							\
			netdev_dbg(dev, fmt, ##__VA_ARGS__);		\
	}								\
} while (0)

#define arc_cont(x, fmt, ...)						\
do {									\
	if (BUGLVL(x))							\
		pr_cont(fmt, ##__VA_ARGS__);				\
} while (0)


#define TIME(dev, name, bytes, call)					\
do {									\
	if (BUGLVL(D_TIMING)) {						\
		unsigned long _x, _y;					\
		_x = get_cycles();					\
		call;							\
		_y = get_cycles();					\
		arc_printk(D_TIMING, dev,				\
			   "%s: %d bytes in %lu cycles == %lu Kbytes/100Mcycle\n", \
			   name, bytes, _y - _x,			\
			   100000000 / 1024 * bytes / (_y - _x + 1));	\
	} else {							\
		call;							\
	}								\
} while (0)


#define RESETtime (300)


#define MTU	253		
#define MinTU	257		
#define XMTU	508		


#define TXFREEflag	0x01	
#define TXACKflag       0x02	
#define RECONflag       0x04	
#define TESTflag        0x08	
#define EXCNAKflag      0x08    
#define RESETflag       0x10	
#define RES1flag        0x20	
#define RES2flag        0x40	
#define NORXflag        0x80	


#define AUTOINCflag     0x40	
#define IOMAPflag       0x02	
#define ENABLE16flag    0x80	


#define NOTXcmd         0x01	
#define NORXcmd         0x02	
#define TXcmd           0x03	
#define RXcmd           0x04	
#define CONFIGcmd       0x05	
#define CFLAGScmd       0x06	
#define TESTcmd         0x07	
#define STARTIOcmd      0x18	


#define RESETclear      0x08	
#define CONFIGclear     0x10	

#define EXCNAKclear     0x0E    


#define TESTload        0x08	


#define TESTvalue       0321	


#define RXbcasts        0x80	


#define NORMALconf      0x00	
#define EXTconf         0x08	


#define ARC_IS_5MBIT    1   
#define ARC_CAN_10MBIT  2   
#define ARC_HAS_LED     4   
#define ARC_HAS_ROTARY  8   


struct ArcProto {
	char suffix;		
	int mtu;		
	int is_ip;              

	void (*rx)(struct net_device *dev, int bufnum,
		   struct archdr *pkthdr, int length);
	int (*build_header)(struct sk_buff *skb, struct net_device *dev,
			    unsigned short ethproto, uint8_t daddr);

	
	int (*prepare_tx)(struct net_device *dev, struct archdr *pkt,
			  int length, int bufnum);
	int (*continue_tx)(struct net_device *dev, int bufnum);
	int (*ack_tx)(struct net_device *dev, int acked);
};

extern struct ArcProto *arc_proto_map[256], *arc_proto_default,
	*arc_bcast_proto, *arc_raw_proto;


struct Incoming {
	struct sk_buff *skb;	
	__be16 sequence;	
	uint8_t lastpacket,	
		numpackets;	
};


struct Outgoing {
	struct ArcProto *proto;	
	struct sk_buff *skb;	
	struct archdr *pkt;	
	uint16_t length,	
		dataleft,	
		segnum,		
		numsegs;	
};

#define ARCNET_LED_NAME_SZ (IFNAMSIZ + 6)

struct arcnet_local {
	uint8_t config,		
		timeout,	
		backplane,	
		clockp,		
		clockm,		
		setup,		
		setup2,		
		intmask;	
	uint8_t default_proto[256];	
	int	cur_tx,		
		next_tx,	
		cur_rx;		
	int	lastload_dest,	
		lasttrans_dest;	
	int	timed_out;	
	unsigned long last_timeout;	
	char *card_name;	
	int card_flags;		

	
	spinlock_t lock;

	struct led_trigger *tx_led_trig;
	char tx_led_trig_name[ARCNET_LED_NAME_SZ];
	struct led_trigger *recon_led_trig;
	char recon_led_trig_name[ARCNET_LED_NAME_SZ];

	struct timer_list	timer;

	struct net_device *dev;
	int reply_status;
	struct tasklet_struct reply_tasklet;

	
	atomic_t buf_lock;
	int buf_queue[5];
	int next_buf, first_free_buf;

	
	unsigned long first_recon; 
	unsigned long last_recon;  
	int num_recons;		
	int network_down;	

	int excnak_pending;    

	
	int reset_in_progress;
	struct work_struct reset_work;

	struct {
		uint16_t sequence;	
		__be16 aborted_seq;

		struct Incoming incoming[256];	
	} rfc1201;

	
	struct Outgoing outgoing;	

	
	struct {
		struct module *owner;
		void (*command)(struct net_device *dev, int cmd);
		int (*status)(struct net_device *dev);
		void (*intmask)(struct net_device *dev, int mask);
		int (*reset)(struct net_device *dev, int really_reset);
		void (*open)(struct net_device *dev);
		void (*close)(struct net_device *dev);
		void (*datatrigger) (struct net_device * dev, int enable);
		void (*recontrigger) (struct net_device * dev, int enable);

		void (*copy_to_card)(struct net_device *dev, int bufnum,
				     int offset, void *buf, int count);
		void (*copy_from_card)(struct net_device *dev, int bufnum,
				       int offset, void *buf, int count);
	} hw;

	void __iomem *mem_start;	
};

enum arcnet_led_event {
	ARCNET_LED_EVENT_RECON,
	ARCNET_LED_EVENT_OPEN,
	ARCNET_LED_EVENT_STOP,
	ARCNET_LED_EVENT_TX,
};

void arcnet_led_event(struct net_device *netdev, enum arcnet_led_event event);
void devm_arcnet_led_init(struct net_device *netdev, int index, int subid);

#if ARCNET_DEBUG_MAX & D_SKB
void arcnet_dump_skb(struct net_device *dev, struct sk_buff *skb, char *desc);
#else
static inline
void arcnet_dump_skb(struct net_device *dev, struct sk_buff *skb, char *desc)
{
}
#endif

void arcnet_unregister_proto(struct ArcProto *proto);
irqreturn_t arcnet_interrupt(int irq, void *dev_id);

struct net_device *alloc_arcdev(const char *name);
void free_arcdev(struct net_device *dev);

int arcnet_open(struct net_device *dev);
int arcnet_close(struct net_device *dev);
netdev_tx_t arcnet_send_packet(struct sk_buff *skb,
			       struct net_device *dev);
void arcnet_timeout(struct net_device *dev, unsigned int txqueue);

static inline void arcnet_set_addr(struct net_device *dev, u8 addr)
{
	dev_addr_set(dev, &addr);
}



#ifdef CONFIG_SA1100_CT6001
#define BUS_ALIGN  2  
#else
#define BUS_ALIGN  1
#endif


#define arcnet_inb(addr, offset)					\
	inb((addr) + BUS_ALIGN * (offset))
#define arcnet_outb(value, addr, offset)				\
	outb(value, (addr) + BUS_ALIGN * (offset))

#define arcnet_insb(addr, offset, buffer, count)			\
	insb((addr) + BUS_ALIGN * (offset), buffer, count)
#define arcnet_outsb(addr, offset, buffer, count)			\
	outsb((addr) + BUS_ALIGN * (offset), buffer, count)

#define arcnet_readb(addr, offset)					\
	readb((addr) + (offset))
#define arcnet_writeb(value, addr, offset)				\
	writeb(value, (addr) + (offset))

#endif				
#endif				
