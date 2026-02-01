 
 

#ifndef __U_SERIAL_H
#define __U_SERIAL_H

#include <linux/usb/composite.h>
#include <linux/usb/cdc.h>

#define MAX_U_SERIAL_PORTS	8

struct f_serial_opts {
	struct usb_function_instance func_inst;
	u8 port_num;
};

 
struct gserial {
	struct usb_function		func;

	 
	struct gs_port			*ioport;

	struct usb_ep			*in;
	struct usb_ep			*out;

	 
	struct usb_cdc_line_coding port_line_coding;	 

	 
	void (*connect)(struct gserial *p);
	void (*disconnect)(struct gserial *p);
	int (*send_break)(struct gserial *p, int duration);
};

 
struct usb_request *gs_alloc_req(struct usb_ep *ep, unsigned len, gfp_t flags);
void gs_free_req(struct usb_ep *, struct usb_request *req);

 
int gserial_alloc_line_no_console(unsigned char *port_line);
int gserial_alloc_line(unsigned char *port_line);
void gserial_free_line(unsigned char port_line);

#ifdef CONFIG_U_SERIAL_CONSOLE

ssize_t gserial_set_console(unsigned char port_num, const char *page, size_t count);
ssize_t gserial_get_console(unsigned char port_num, char *page);

#endif  

 
int gserial_connect(struct gserial *, u8 port_num);
void gserial_disconnect(struct gserial *);
void gserial_suspend(struct gserial *p);
void gserial_resume(struct gserial *p);

#endif  
