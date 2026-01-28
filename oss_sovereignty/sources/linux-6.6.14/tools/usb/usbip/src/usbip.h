


#ifndef __USBIP_H
#define __USBIP_H

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif


int usbip_attach(int argc, char *argv[]);
int usbip_detach(int argc, char *argv[]);
int usbip_list(int argc, char *argv[]);
int usbip_bind(int argc, char *argv[]);
int usbip_unbind(int argc, char *argv[]);
int usbip_port_show(int argc, char *argv[]);

void usbip_attach_usage(void);
void usbip_detach_usage(void);
void usbip_list_usage(void);
void usbip_bind_usage(void);
void usbip_unbind_usage(void);

#endif 
