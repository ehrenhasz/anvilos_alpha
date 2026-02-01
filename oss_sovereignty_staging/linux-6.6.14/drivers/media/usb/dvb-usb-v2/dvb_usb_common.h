 
 

#ifndef DVB_USB_COMMON_H
#define DVB_USB_COMMON_H

#include "dvb_usb.h"

 
extern int usb_urb_initv2(struct usb_data_stream *stream,
		const struct usb_data_stream_properties *props);
extern int usb_urb_exitv2(struct usb_data_stream *stream);
extern int usb_urb_submitv2(struct usb_data_stream *stream,
		struct usb_data_stream_properties *props);
extern int usb_urb_killv2(struct usb_data_stream *stream);

#endif
