
#ifndef MICROPY_INCLUDED_ESP32_USB_H
#define MICROPY_INCLUDED_ESP32_USB_H

#define MICROPY_HW_USB_CDC_TX_TIMEOUT_MS (500)

void usb_init(void);
void usb_tx_strn(const char *str, size_t len);

#endif 
