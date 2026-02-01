 

#include "py/runtime.h"
#include "py/mphal.h"
#include "extmod/modmachine.h"

#if MICROPY_HW_USB_CDC_1200BPS_TOUCH && MICROPY_HW_ENABLE_USBDEV

#include "tusb.h"

static mp_sched_node_t mp_bootloader_sched_node;

static void usbd_cdc_run_bootloader_task(mp_sched_node_t *node) {
    mp_hal_delay_ms(250);
    machine_bootloader(0, NULL);
}

void
#if MICROPY_HW_USB_EXTERNAL_TINYUSB
mp_usbd_line_state_cb
#else
tud_cdc_line_state_cb
#endif
    (uint8_t itf, bool dtr, bool rts) {
    if (dtr == false && rts == false) {
        
        cdc_line_coding_t line_coding;
        tud_cdc_n_get_line_coding(itf, &line_coding);
        if (line_coding.bit_rate == 1200) {
            
            mp_sched_schedule_node(&mp_bootloader_sched_node, usbd_cdc_run_bootloader_task);
        }
    }
}

#endif
