




#define ACM_TTY_MAJOR		166
#define ACM_TTY_MINORS		256

#define ACM_MINOR_INVALID	ACM_TTY_MINORS



#define USB_RT_ACM		(USB_TYPE_CLASS | USB_RECIP_INTERFACE)




#define ACM_NW  16
#define ACM_NR  16

struct acm_wb {
	u8 *buf;
	dma_addr_t dmah;
	unsigned int len;
	struct urb		*urb;
	struct acm		*instance;
	bool use;
};

struct acm_rb {
	int			size;
	unsigned char		*base;
	dma_addr_t		dma;
	int			index;
	struct acm		*instance;
};

struct acm {
	struct usb_device *dev;				
	struct usb_interface *control;			
	struct usb_interface *data;			
	unsigned in, out;				
	struct tty_port port;			 	
	struct urb *ctrlurb;				
	u8 *ctrl_buffer;				
	dma_addr_t ctrl_dma;				
	u8 *country_codes;				
	unsigned int country_code_size;			
	unsigned int country_rel_date;			
	struct acm_wb wb[ACM_NW];
	unsigned long read_urbs_free;
	struct urb *read_urbs[ACM_NR];
	struct acm_rb read_buffers[ACM_NR];
	int rx_buflimit;
	spinlock_t read_lock;
	u8 *notification_buffer;			
	unsigned int nb_index;
	unsigned int nb_size;
	int transmitting;
	spinlock_t write_lock;
	struct mutex mutex;
	bool disconnected;
	unsigned long flags;
#		define EVENT_TTY_WAKEUP	0
#		define EVENT_RX_STALL	1
#		define ACM_THROTTLED	2
#		define ACM_ERROR_DELAY	3
	unsigned long urbs_in_error_delay;		
	struct usb_cdc_line_coding line;		
	struct delayed_work dwork;		        
	unsigned int ctrlin;				
	unsigned int ctrlout;				
	struct async_icount iocount;			
	struct async_icount oldcount;			
	wait_queue_head_t wioctl;			
	unsigned int writesize;				
	unsigned int readsize,ctrlsize;			
	unsigned int minor;				
	unsigned char clocal;				
	unsigned int ctrl_caps;				
	unsigned int susp_count;			
	unsigned int combined_interfaces:1;		
	u8 bInterval;
	struct usb_anchor delayed;			
	unsigned long quirks;
};


#define NO_UNION_NORMAL			BIT(0)
#define SINGLE_RX_URB			BIT(1)
#define NO_CAP_LINE			BIT(2)
#define IGNORE_DEVICE			BIT(3)
#define QUIRK_CONTROL_LINE_STATE	BIT(4)
#define CLEAR_HALT_CONDITIONS		BIT(5)
#define SEND_ZERO_PACKET		BIT(6)
#define DISABLE_ECHO			BIT(7)
