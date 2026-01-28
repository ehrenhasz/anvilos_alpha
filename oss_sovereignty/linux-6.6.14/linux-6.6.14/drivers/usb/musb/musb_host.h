#ifndef _MUSB_HOST_H
#define _MUSB_HOST_H
#include <linux/scatterlist.h>
struct musb_qh {
	struct usb_host_endpoint *hep;		 
	struct usb_device	*dev;
	struct musb_hw_ep	*hw_ep;		 
	struct list_head	ring;		 
	u8			mux;		 
	unsigned		offset;		 
	unsigned		segsize;	 
	u8			type_reg;	 
	u8			intv_reg;	 
	u8			addr_reg;	 
	u8			h_addr_reg;	 
	u8			h_port_reg;	 
	u8			is_ready;	 
	u8			type;		 
	u8			epnum;
	u8			hb_mult;	 
	u16			maxpacket;
	u16			frame;		 
	unsigned		iso_idx;	 
	struct sg_mapping_iter sg_miter;	 
	bool			use_sg;		 
};
static inline struct musb_qh *first_qh(struct list_head *q)
{
	if (list_empty(q))
		return NULL;
	return list_entry(q->next, struct musb_qh, ring);
}
#if IS_ENABLED(CONFIG_USB_MUSB_HOST) || IS_ENABLED(CONFIG_USB_MUSB_DUAL_ROLE)
extern struct musb *hcd_to_musb(struct usb_hcd *);
extern irqreturn_t musb_h_ep0_irq(struct musb *);
extern int musb_host_alloc(struct musb *);
extern int musb_host_setup(struct musb *, int);
extern void musb_host_cleanup(struct musb *);
extern void musb_host_tx(struct musb *, u8);
extern void musb_host_rx(struct musb *, u8);
extern void musb_root_disconnect(struct musb *musb);
extern void musb_host_free(struct musb *);
extern void musb_host_resume_root_hub(struct musb *musb);
extern void musb_host_poke_root_hub(struct musb *musb);
extern int musb_port_suspend(struct musb *musb, bool do_suspend);
extern void musb_port_reset(struct musb *musb, bool do_reset);
extern void musb_host_finish_resume(struct work_struct *work);
#else
static inline struct musb *hcd_to_musb(struct usb_hcd *hcd)
{
	return NULL;
}
static inline irqreturn_t musb_h_ep0_irq(struct musb *musb)
{
	return 0;
}
static inline int musb_host_alloc(struct musb *musb)
{
	return 0;
}
static inline int musb_host_setup(struct musb *musb, int power_budget)
{
	return 0;
}
static inline void musb_host_cleanup(struct musb *musb)		{}
static inline void musb_host_free(struct musb *musb)		{}
static inline void musb_host_tx(struct musb *musb, u8 epnum)	{}
static inline void musb_host_rx(struct musb *musb, u8 epnum)	{}
static inline void musb_root_disconnect(struct musb *musb)	{}
static inline void musb_host_resume_root_hub(struct musb *musb)	{}
static inline void musb_host_poke_root_hub(struct musb *musb)	{}
static inline int musb_port_suspend(struct musb *musb, bool do_suspend)
{
	return 0;
}
static inline void musb_port_reset(struct musb *musb, bool do_reset) {}
static inline void musb_host_finish_resume(struct work_struct *work) {}
#endif
struct usb_hcd;
extern int musb_hub_status_data(struct usb_hcd *hcd, char *buf);
extern int musb_hub_control(struct usb_hcd *hcd,
			u16 typeReq, u16 wValue, u16 wIndex,
			char *buf, u16 wLength);
static inline struct urb *next_urb(struct musb_qh *qh)
{
	struct list_head	*queue;
	if (!qh)
		return NULL;
	queue = &qh->hep->urb_list;
	if (list_empty(queue))
		return NULL;
	return list_entry(queue->next, struct urb, urb_list);
}
#endif				 
