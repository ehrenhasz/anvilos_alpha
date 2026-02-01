 
 

#ifndef _F_UVC_H_
#define _F_UVC_H_

struct uvc_device;

void uvc_function_setup_continue(struct uvc_device *uvc);

void uvc_function_connect(struct uvc_device *uvc);

void uvc_function_disconnect(struct uvc_device *uvc);

#endif  
