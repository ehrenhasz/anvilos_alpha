

#ifndef NRF_DRIVERS_USB_CDC_H__
#define NRF_DRIVERS_USB_CDC_H__

#include "tusb.h"

void usb_cdc_init(void);

void usb_cdc_loop(void);
int  usb_cdc_read_char(void);
void usb_cdc_write_char(char c);

void usb_cdc_sd_event_handler(uint32_t soc_evt);

#endif 
