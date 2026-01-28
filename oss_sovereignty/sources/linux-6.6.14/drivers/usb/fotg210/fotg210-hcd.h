
#ifndef __LINUX_FOTG210_H
#define __LINUX_FOTG210_H

#include <linux/usb/ehci-dbgp.h>




#define __hc32	__le32
#define __hc16	__le16


struct fotg210_stats {
	
	unsigned long		normal;
	unsigned long		error;
	unsigned long		iaa;
	unsigned long		lost_iaa;

	
	unsigned long		complete;
	unsigned long		unlink;
};



#define	FOTG210_MAX_ROOT_PORTS	1		


enum fotg210_rh_state {
	FOTG210_RH_HALTED,
	FOTG210_RH_SUSPENDED,
	FOTG210_RH_RUNNING,
	FOTG210_RH_STOPPING
};


enum fotg210_hrtimer_event {
	FOTG210_HRTIMER_POLL_ASS,	
	FOTG210_HRTIMER_POLL_PSS,	
	FOTG210_HRTIMER_POLL_DEAD,	
	FOTG210_HRTIMER_UNLINK_INTR,	
	FOTG210_HRTIMER_FREE_ITDS,	
	FOTG210_HRTIMER_ASYNC_UNLINKS,	
	FOTG210_HRTIMER_IAA_WATCHDOG,	
	FOTG210_HRTIMER_DISABLE_PERIODIC, 
	FOTG210_HRTIMER_DISABLE_ASYNC,	
	FOTG210_HRTIMER_IO_WATCHDOG,	
	FOTG210_HRTIMER_NUM_EVENTS	
};
#define FOTG210_HRTIMER_NO_EVENT	99

struct fotg210_hcd {			
	
	enum fotg210_hrtimer_event	next_hrtimer_event;
	unsigned		enabled_hrtimer_events;
	ktime_t			hr_timeouts[FOTG210_HRTIMER_NUM_EVENTS];
	struct hrtimer		hrtimer;

	int			PSS_poll_count;
	int			ASS_poll_count;
	int			died_poll_count;

	
	struct fotg210_caps __iomem *caps;
	struct fotg210_regs __iomem *regs;
	struct ehci_dbg_port __iomem *debug;

	__u32			hcs_params;	
	spinlock_t		lock;
	enum fotg210_rh_state	rh_state;

	
	bool			scanning:1;
	bool			need_rescan:1;
	bool			intr_unlinking:1;
	bool			async_unlinking:1;
	bool			shutdown:1;
	struct fotg210_qh		*qh_scan_next;

	
	struct fotg210_qh		*async;
	struct fotg210_qh		*dummy;		
	struct fotg210_qh		*async_unlink;
	struct fotg210_qh		*async_unlink_last;
	struct fotg210_qh		*async_iaa;
	unsigned		async_unlink_cycle;
	unsigned		async_count;	

	
#define	DEFAULT_I_TDPS		1024		
	unsigned		periodic_size;
	__hc32			*periodic;	
	dma_addr_t		periodic_dma;
	struct list_head	intr_qh_list;
	unsigned		i_thresh;	

	union fotg210_shadow	*pshadow;	
	struct fotg210_qh		*intr_unlink;
	struct fotg210_qh		*intr_unlink_last;
	unsigned		intr_unlink_cycle;
	unsigned		now_frame;	
	unsigned		next_frame;	
	unsigned		intr_count;	
	unsigned		isoc_count;	
	unsigned		periodic_count;	
	
	unsigned		uframe_periodic_max;


	
	struct list_head	cached_itd_list;
	struct fotg210_itd	*last_itd_to_free;

	
	unsigned long		reset_done[FOTG210_MAX_ROOT_PORTS];

	
	unsigned long		bus_suspended;

	
	unsigned long		companion_ports;

	
	unsigned long		owned_ports;

	
	unsigned long		port_c_suspend;

	
	unsigned long		suspended_ports;

	
	unsigned long		resuming_ports;

	
	struct dma_pool		*qh_pool;	
	struct dma_pool		*qtd_pool;	
	struct dma_pool		*itd_pool;	

	unsigned		random_frame;
	unsigned long		next_statechange;
	ktime_t			last_periodic_enable;
	u32			command;

	
	unsigned		need_io_watchdog:1;
	unsigned		fs_i_thresh:1;	

	u8			sbrn;		

	
#ifdef FOTG210_STATS
	struct fotg210_stats	stats;
#	define INCR(x) ((x)++)
#else
#	define INCR(x) do {} while (0)
#endif

	struct fotg210		*fotg;		
	
	struct clk		*pclk;
};


static inline struct fotg210_hcd *hcd_to_fotg210(struct usb_hcd *hcd)
{
	return (struct fotg210_hcd *)(hcd->hcd_priv);
}
static inline struct usb_hcd *fotg210_to_hcd(struct fotg210_hcd *fotg210)
{
	return container_of((void *) fotg210, struct usb_hcd, hcd_priv);
}






struct fotg210_caps {
	
	u32		hc_capbase;
#define HC_LENGTH(fotg210, p)	(0x00ff&((p) >>  \
				(fotg210_big_endian_capbase(fotg210) ? 24 : 0)))
#define HC_VERSION(fotg210, p)	(0xffff&((p) >>  \
				(fotg210_big_endian_capbase(fotg210) ? 0 : 16)))
	u32		hcs_params;     
#define HCS_N_PORTS(p)		(((p)>>0)&0xf)	

	u32		hcc_params;	
#define HCC_CANPARK(p)		((p)&(1 << 2))  
#define HCC_PGM_FRAMELISTLEN(p) ((p)&(1 << 1))  
	u8		portroute[8];	 
};



struct fotg210_regs {

	
	u32		command;



#define CMD_PARK	(1<<11)		
#define CMD_PARK_CNT(c)	(((c)>>8)&3)	
#define CMD_IAAD	(1<<6)		
#define CMD_ASE		(1<<5)		
#define CMD_PSE		(1<<4)		

#define CMD_RESET	(1<<1)		
#define CMD_RUN		(1<<0)		

	
	u32		status;
#define STS_ASS		(1<<15)		
#define STS_PSS		(1<<14)		
#define STS_RECL	(1<<13)		
#define STS_HALT	(1<<12)		

	
#define STS_IAA		(1<<5)		
#define STS_FATAL	(1<<4)		
#define STS_FLR		(1<<3)		
#define STS_PCD		(1<<2)		
#define STS_ERR		(1<<1)		
#define STS_INT		(1<<0)		

	
	u32		intr_enable;

	
	u32		frame_index;	
	
	u32		segment;	
	
	u32		frame_list;	
	
	u32		async_next;	

	u32	reserved1;
	
	u32	port_status;

#define PORT_USB11(x) (((x)&(3<<10)) == (1<<10))	
#define PORT_RESET	(1<<8)		
#define PORT_SUSPEND	(1<<7)		
#define PORT_RESUME	(1<<6)		
#define PORT_PEC	(1<<3)		
#define PORT_PE		(1<<2)		
#define PORT_CSC	(1<<1)		
#define PORT_CONNECT	(1<<0)		
#define PORT_RWC_BITS   (PORT_CSC | PORT_PEC)
	u32     reserved2[19];

	
	u32     otgcsr;
#define OTGCSR_HOST_SPD_TYP     (3 << 22)
#define OTGCSR_A_BUS_DROP	(1 << 5)
#define OTGCSR_A_BUS_REQ	(1 << 4)

	
	u32     otgisr;
#define OTGISR_OVC	(1 << 10)

	u32     reserved3[15];

	
	u32     gmir;
#define GMIR_INT_POLARITY	(1 << 3) 
#define GMIR_MHC_INT		(1 << 2)
#define GMIR_MOTG_INT		(1 << 1)
#define GMIR_MDEV_INT	(1 << 0)
};



#define	QTD_NEXT(fotg210, dma)	cpu_to_hc32(fotg210, (u32)dma)


struct fotg210_qtd {
	
	__hc32			hw_next;	
	__hc32			hw_alt_next;    
	__hc32			hw_token;	
#define	QTD_TOGGLE	(1 << 31)	
#define	QTD_LENGTH(tok)	(((tok)>>16) & 0x7fff)
#define	QTD_IOC		(1 << 15)	
#define	QTD_CERR(tok)	(((tok)>>10) & 0x3)
#define	QTD_PID(tok)	(((tok)>>8) & 0x3)
#define	QTD_STS_ACTIVE	(1 << 7)	
#define	QTD_STS_HALT	(1 << 6)	
#define	QTD_STS_DBE	(1 << 5)	
#define	QTD_STS_BABBLE	(1 << 4)	
#define	QTD_STS_XACT	(1 << 3)	
#define	QTD_STS_MMF	(1 << 2)	
#define	QTD_STS_STS	(1 << 1)	
#define	QTD_STS_PING	(1 << 0)	

#define ACTIVE_BIT(fotg210)	cpu_to_hc32(fotg210, QTD_STS_ACTIVE)
#define HALT_BIT(fotg210)		cpu_to_hc32(fotg210, QTD_STS_HALT)
#define STATUS_BIT(fotg210)	cpu_to_hc32(fotg210, QTD_STS_STS)

	__hc32			hw_buf[5];	
	__hc32			hw_buf_hi[5];	

	
	dma_addr_t		qtd_dma;		
	struct list_head	qtd_list;		
	struct urb		*urb;			
	size_t			length;			
} __aligned(32);


#define QTD_MASK(fotg210)	cpu_to_hc32(fotg210, ~0x1f)

#define IS_SHORT_READ(token) (QTD_LENGTH(token) != 0 && QTD_PID(token) == 1)




#define Q_NEXT_TYPE(fotg210, dma)	((dma) & cpu_to_hc32(fotg210, 3 << 1))



#define Q_TYPE_ITD	(0 << 1)
#define Q_TYPE_QH	(1 << 1)
#define Q_TYPE_SITD	(2 << 1)
#define Q_TYPE_FSTN	(3 << 1)


#define QH_NEXT(fotg210, dma) \
	(cpu_to_hc32(fotg210, (((u32)dma)&~0x01f)|Q_TYPE_QH))


#define FOTG210_LIST_END(fotg210) \
	cpu_to_hc32(fotg210, 1) 


union fotg210_shadow {
	struct fotg210_qh	*qh;		
	struct fotg210_itd	*itd;		
	struct fotg210_fstn	*fstn;		
	__hc32			*hw_next;	
	void			*ptr;
};






struct fotg210_qh_hw {
	__hc32			hw_next;	
	__hc32			hw_info1;	
#define	QH_CONTROL_EP	(1 << 27)	
#define	QH_HEAD		(1 << 15)	
#define	QH_TOGGLE_CTL	(1 << 14)	
#define	QH_HIGH_SPEED	(2 << 12)	
#define	QH_LOW_SPEED	(1 << 12)
#define	QH_FULL_SPEED	(0 << 12)
#define	QH_INACTIVATE	(1 << 7)	
	__hc32			hw_info2;	
#define	QH_SMASK	0x000000ff
#define	QH_CMASK	0x0000ff00
#define	QH_HUBADDR	0x007f0000
#define	QH_HUBPORT	0x3f800000
#define	QH_MULT		0xc0000000
	__hc32			hw_current;	

	
	__hc32			hw_qtd_next;
	__hc32			hw_alt_next;
	__hc32			hw_token;
	__hc32			hw_buf[5];
	__hc32			hw_buf_hi[5];
} __aligned(32);

struct fotg210_qh {
	struct fotg210_qh_hw	*hw;		
	
	dma_addr_t		qh_dma;		
	union fotg210_shadow	qh_next;	
	struct list_head	qtd_list;	
	struct list_head	intr_node;	
	struct fotg210_qtd	*dummy;
	struct fotg210_qh	*unlink_next;	

	unsigned		unlink_cycle;

	u8			needs_rescan;	
	u8			qh_state;
#define	QH_STATE_LINKED		1		
#define	QH_STATE_UNLINK		2		
#define	QH_STATE_IDLE		3		
#define	QH_STATE_UNLINK_WAIT	4		
#define	QH_STATE_COMPLETING	5		

	u8			xacterrs;	
#define	QH_XACTERR_MAX		32		

	
	u8			usecs;		
	u8			gap_uf;		
	u8			c_usecs;	
	u16			tt_usecs;	
	unsigned short		period;		
	unsigned short		start;		
#define NO_FRAME ((unsigned short)~0)			

	struct usb_device	*dev;		
	unsigned		is_out:1;	
	unsigned		clearing_tt:1;	
};




struct fotg210_iso_packet {
	
	u64			bufp;		
	__hc32			transaction;	
	u8			cross;		
	
	u32			buf1;
};


struct fotg210_iso_sched {
	struct list_head	td_list;
	unsigned		span;
	struct fotg210_iso_packet	packet[];
};


struct fotg210_iso_stream {
	
	struct fotg210_qh_hw	*hw;

	u8			bEndpointAddress;
	u8			highspeed;
	struct list_head	td_list;	
	struct list_head	free_list;	
	struct usb_device	*udev;
	struct usb_host_endpoint *ep;

	
	int			next_uframe;
	__hc32			splits;

	
	u8			usecs, c_usecs;
	u16			interval;
	u16			tt_usecs;
	u16			maxp;
	u16			raw_mask;
	unsigned		bandwidth;

	
	__hc32			buf0;
	__hc32			buf1;
	__hc32			buf2;

	
	__hc32			address;
};




struct fotg210_itd {
	
	__hc32			hw_next;	
	__hc32			hw_transaction[8]; 
#define FOTG210_ISOC_ACTIVE	(1<<31)	
#define FOTG210_ISOC_BUF_ERR	(1<<30)	
#define FOTG210_ISOC_BABBLE	(1<<29)	
#define FOTG210_ISOC_XACTERR	(1<<28)	
#define	FOTG210_ITD_LENGTH(tok)	(((tok)>>16) & 0x0fff)
#define	FOTG210_ITD_IOC		(1 << 15)	

#define ITD_ACTIVE(fotg210)	cpu_to_hc32(fotg210, FOTG210_ISOC_ACTIVE)

	__hc32			hw_bufp[7];	
	__hc32			hw_bufp_hi[7];	

	
	dma_addr_t		itd_dma;	
	union fotg210_shadow	itd_next;	

	struct urb		*urb;
	struct fotg210_iso_stream	*stream;	
	struct list_head	itd_list;	

	
	unsigned		frame;		
	unsigned		pg;
	unsigned		index[8];	
} __aligned(32);




struct fotg210_fstn {
	__hc32			hw_next;	
	__hc32			hw_prev;	

	
	dma_addr_t		fstn_dma;
	union fotg210_shadow	fstn_next;	
} __aligned(32);





#define fotg210_prepare_ports_for_controller_suspend(fotg210, do_wakeup) \
		fotg210_adjust_port_wakeup_flags(fotg210, true, do_wakeup)

#define fotg210_prepare_ports_for_controller_resume(fotg210)		\
		fotg210_adjust_port_wakeup_flags(fotg210, false, false)





static inline unsigned int
fotg210_get_speed(struct fotg210_hcd *fotg210, unsigned int portsc)
{
	return (readl(&fotg210->regs->otgcsr)
		& OTGCSR_HOST_SPD_TYP) >> 22;
}


static inline unsigned int
fotg210_port_speed(struct fotg210_hcd *fotg210, unsigned int portsc)
{
	switch (fotg210_get_speed(fotg210, portsc)) {
	case 0:
		return 0;
	case 1:
		return USB_PORT_STAT_LOW_SPEED;
	case 2:
	default:
		return USB_PORT_STAT_HIGH_SPEED;
	}
}



#define	fotg210_has_fsl_portno_bug(e)		(0)



#define fotg210_big_endian_mmio(e)	0
#define fotg210_big_endian_capbase(e)	0

static inline unsigned int fotg210_readl(const struct fotg210_hcd *fotg210,
		__u32 __iomem *regs)
{
	return readl(regs);
}

static inline void fotg210_writel(const struct fotg210_hcd *fotg210,
		const unsigned int val, __u32 __iomem *regs)
{
	writel(val, regs);
}


static inline __hc32 cpu_to_hc32(const struct fotg210_hcd *fotg210, const u32 x)
{
	return cpu_to_le32(x);
}


static inline u32 hc32_to_cpu(const struct fotg210_hcd *fotg210, const __hc32 x)
{
	return le32_to_cpu(x);
}

static inline u32 hc32_to_cpup(const struct fotg210_hcd *fotg210,
			       const __hc32 *x)
{
	return le32_to_cpup(x);
}



static inline unsigned fotg210_read_frame_index(struct fotg210_hcd *fotg210)
{
	return fotg210_readl(fotg210, &fotg210->regs->frame_index);
}



#endif 
