
#ifndef MICROPY_INCLUDED_ESP32_USB_SERIAL_JTAG_H
#define MICROPY_INCLUDED_ESP32_USB_SERIAL_JTAG_H

void usb_serial_jtag_init(void);
void usb_serial_jtag_poll_rx(void);
void usb_serial_jtag_tx_strn(const char *str, size_t len);

#endif 
