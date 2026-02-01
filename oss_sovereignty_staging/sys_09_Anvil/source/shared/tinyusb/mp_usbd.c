 

#include "py/mpconfig.h"

#if MICROPY_HW_ENABLE_USBDEV

#include "mp_usbd.h"

#ifndef NO_QSTR
#include "device/dcd.h"
#endif

#if !MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE

void mp_usbd_task(void) {
    tud_task_ext(0, false);
}

void mp_usbd_task_callback(mp_sched_node_t *node) {
    (void)node;
    mp_usbd_task();
}

#endif 

extern void __real_dcd_event_handler(dcd_event_t const *event, bool in_isr);




TU_ATTR_FAST_FUNC void __wrap_dcd_event_handler(dcd_event_t const *event, bool in_isr) {
    __real_dcd_event_handler(event, in_isr);
    mp_usbd_schedule_task();
}

TU_ATTR_FAST_FUNC void mp_usbd_schedule_task(void) {
    static mp_sched_node_t usbd_task_node;
    mp_sched_schedule_node(&usbd_task_node, mp_usbd_task_callback);
}

void mp_usbd_hex_str(char *out_str, const uint8_t *bytes, size_t bytes_len) {
    size_t hex_len = bytes_len * 2;
    for (int i = 0; i < hex_len; i += 2) {
        static const char *hexdig = "0123456789abcdef";
        out_str[i] = hexdig[bytes[i / 2] >> 4];
        out_str[i + 1] = hexdig[bytes[i / 2] & 0x0f];
    }
    out_str[hex_len] = 0;
}

#endif 
