
#ifndef MICROPY_INCLUDED_STM32_USBD_CDC_INTERFACE_H
#define MICROPY_INCLUDED_STM32_USBD_CDC_INTERFACE_H



#include "py/mpconfig.h"


#define USBD_CDC_CONNECT_STATE_DISCONNECTED (0)
#define USBD_CDC_CONNECT_STATE_CONNECTING (1)
#define USBD_CDC_CONNECT_STATE_CONNECTED (2)


#define USBD_CDC_FLOWCONTROL_NONE (0)
#define USBD_CDC_FLOWCONTROL_RTS (1)
#define USBD_CDC_FLOWCONTROL_CTS (2)

typedef struct _usbd_cdc_itf_t {
    usbd_cdc_state_t base; 

    uint8_t rx_packet_buf[CDC_DATA_MAX_PACKET_SIZE]; 
    uint8_t rx_user_buf[MICROPY_HW_USB_CDC_RX_DATA_SIZE]; 
    volatile uint16_t rx_buf_put; 
    uint16_t rx_buf_get; 
    uint8_t rx_buf_full; 

    uint8_t tx_buf[MICROPY_HW_USB_CDC_TX_DATA_SIZE]; 
    uint16_t tx_buf_ptr_in; 
    volatile uint16_t tx_buf_ptr_out; 
    uint16_t tx_buf_ptr_out_next; 
    uint8_t tx_need_empty_packet; 

    uint8_t cdc_idx; 
    volatile uint8_t connect_state; 
    uint8_t attached_to_repl; 
    uint8_t flow; 
    uint32_t bitrate;
} usbd_cdc_itf_t;


usbd_cdc_itf_t *usb_vcp_get(int idx);

static inline int usbd_cdc_is_connected(usbd_cdc_itf_t *cdc) {
    return cdc->connect_state == USBD_CDC_CONNECT_STATE_CONNECTED;
}

int usbd_cdc_tx_half_empty(usbd_cdc_itf_t *cdc);
int usbd_cdc_tx_flow(usbd_cdc_itf_t *cdc, const uint8_t *buf, uint32_t len);
int usbd_cdc_tx(usbd_cdc_itf_t *cdc, const uint8_t *buf, uint32_t len, uint32_t timeout);
void usbd_cdc_tx_always(usbd_cdc_itf_t *cdc, const uint8_t *buf, uint32_t len);

int usbd_cdc_rx_num(usbd_cdc_itf_t *cdc);
int usbd_cdc_rx(usbd_cdc_itf_t *cdc, uint8_t *buf, uint32_t len, uint32_t timeout);
void usbd_cdc_rx_event_callback(usbd_cdc_itf_t *cdc);

#endif 
