 

#ifndef MICROPY_INCLUDED_SHARED_TINYUSB_MP_USBD_H
#define MICROPY_INCLUDED_SHARED_TINYUSB_MP_USBD_H

#include "py/mpconfig.h"

#if MICROPY_HW_ENABLE_USBDEV

#include "py/obj.h"
#include "py/objarray.h"
#include "py/runtime.h"

#ifndef NO_QSTR
#include "tusb.h"
#include "device/dcd.h"
#endif


void mp_usbd_task(void);


void mp_usbd_schedule_task(void);



extern void mp_usbd_port_get_serial_number(char *buf);




void mp_usbd_hex_str(char *out_str, const uint8_t *bytes, size_t bytes_len);


#define MP_USBD_BUILTIN_DESC_CFG_LEN (TUD_CONFIG_DESC_LEN +                     \
    (CFG_TUD_CDC ? (TUD_CDC_DESC_LEN) : 0) +  \
    (CFG_TUD_MSC ? (TUD_MSC_DESC_LEN) : 0)    \
    )


extern const tusb_desc_device_t mp_usbd_builtin_desc_dev;
extern const uint8_t mp_usbd_builtin_desc_cfg[MP_USBD_BUILTIN_DESC_CFG_LEN];

void mp_usbd_task_callback(mp_sched_node_t *node);

#if MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE
void mp_usbd_deinit(void);
void mp_usbd_init(void);

const char *mp_usbd_runtime_string_cb(uint8_t index);


#define MP_USBD_MAX_PEND_EXCS 2

typedef struct {
    mp_obj_base_t base;

    mp_obj_t desc_dev; 
    mp_obj_t desc_cfg; 
    mp_obj_t desc_strs; 

    
    mp_obj_t open_itf_cb;
    mp_obj_t reset_cb;
    mp_obj_t control_xfer_cb;
    mp_obj_t xfer_cb;

    mp_obj_t builtin_driver; 

    bool active; 
    bool trigger; 

    
    
    mp_obj_t xfer_data[CFG_TUD_ENDPPOINT_MAX][2];

    
    
    
    
    mp_obj_array_t *control_data;

    
    
    mp_uint_t num_pend_excs;
    mp_obj_t pend_excs[MP_USBD_MAX_PEND_EXCS];
} mp_obj_usb_device_t;




extern const mp_obj_type_t mp_type_usb_device_builtin_default;
extern const mp_obj_type_t mp_type_usb_device_builtin_none;


inline static bool mp_usb_device_builtin_enabled(const mp_obj_usb_device_t *usbd) {
    return usbd->builtin_driver != MP_OBJ_FROM_PTR(&mp_type_usb_device_builtin_none);
}

#else 

static inline void mp_usbd_init(void) {
    
    extern bool tusb_init(void);
    tusb_init();
}

#endif

#endif 

#endif 
