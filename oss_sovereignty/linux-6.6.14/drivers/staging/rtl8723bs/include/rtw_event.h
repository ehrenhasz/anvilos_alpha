 
 
#ifndef _RTW_EVENT_H_
#define _RTW_EVENT_H_

 
struct survey_event	{
	struct wlan_bssid_ex bss;
};

 
struct surveydone_event {
	unsigned int	bss_cnt;

};

 
struct joinbss_event {
	struct	wlan_network	network;
};

 
struct stassoc_event {
	unsigned char macaddr[6];
	unsigned char rsvd[2];
	int    cam_id;

};

struct stadel_event {
 unsigned char macaddr[6];
 unsigned char rsvd[2];  
 int mac_id;
};

struct wmm_event {
	unsigned char wmm;
};

#define GEN_EVT_CODE(event)	event ## _EVT_



struct fwevent {
	u32 parmsize;
	void (*event_callback)(struct adapter *dev, u8 *pbuf);
};


#define C2HEVENT_SZ			32

struct event_node {
	unsigned char *node;
	unsigned char evt_code;
	unsigned short evt_sz;
	volatile int	*caller_ff_tail;
	int	caller_ff_sz;
};

#define NETWORK_QUEUE_SZ	4

struct network_queue {
	volatile int	head;
	volatile int	tail;
	struct wlan_bssid_ex networks[NETWORK_QUEUE_SZ];
};


#endif  
