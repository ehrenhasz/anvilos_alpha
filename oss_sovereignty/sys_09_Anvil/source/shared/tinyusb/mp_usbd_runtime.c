 
#include <stdlib.h>

#include "mp_usbd.h"
#include "py/mpconfig.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/obj.h"
#include "py/objarray.h"
#include "py/objstr.h"
#include "py/runtime.h"

#if MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE

#ifndef NO_QSTR
#include "tusb.h" 
#include "device/dcd.h"
#include "device/usbd.h"
#include "device/usbd_pvt.h"
#endif

static bool in_usbd_task; 



static void mp_usbd_disconnect(mp_obj_usb_device_t *usbd);
static void mp_usbd_task_inner(void);















static void usbd_pend_exception(mp_obj_t exception) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    assert(usbd != NULL);
    if (usbd->num_pend_excs < MP_USBD_MAX_PEND_EXCS) {
        usbd->pend_excs[usbd->num_pend_excs] = exception;
    }
    usbd->num_pend_excs++;
}




static mp_obj_t usbd_callback_function_n(mp_obj_t fun, size_t n_args, const mp_obj_t *args) {
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {
        mp_obj_t ret = mp_call_function_n_kw(fun, n_args, 0, args);
        nlr_pop();
        return ret;
    } else {
        usbd_pend_exception(MP_OBJ_FROM_PTR(nlr.ret_val));
        return MP_OBJ_NULL;
    }
}


static void *usbd_get_buffer_in_cb(mp_obj_t obj, mp_uint_t flags) {
    mp_buffer_info_t buf_info;
    if (obj == mp_const_none) {
        
        return NULL;
    } else if (mp_get_buffer(obj, &buf_info, flags)) {
        return buf_info.buf;
    } else {
        mp_obj_t exc = mp_obj_new_exception_msg(&mp_type_TypeError,
            MP_ERROR_TEXT("object with buffer protocol required"));
        usbd_pend_exception(exc);
        return NULL;
    }
}

const uint8_t *tud_descriptor_device_cb(void) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    const void *result = NULL;
    if (usbd) {
        result = usbd_get_buffer_in_cb(usbd->desc_dev, MP_BUFFER_READ);
    }
    return result ? result : &mp_usbd_builtin_desc_dev;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    const void *result = NULL;
    if (usbd) {
        result = usbd_get_buffer_in_cb(usbd->desc_cfg, MP_BUFFER_READ);
    }
    return result ? result : &mp_usbd_builtin_desc_cfg;
}

const char *mp_usbd_runtime_string_cb(uint8_t index) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    nlr_buf_t nlr;

    if (usbd == NULL || usbd->desc_strs == mp_const_none) {
        return NULL;
    }

    if (nlr_push(&nlr) == 0) {
        mp_obj_t res = mp_obj_subscr(usbd->desc_strs, mp_obj_new_int(index), MP_OBJ_SENTINEL);
        nlr_pop();
        if (res != mp_const_none) {
            return usbd_get_buffer_in_cb(res, MP_BUFFER_READ);
        }
    } else {
        mp_obj_t exception = MP_OBJ_FROM_PTR(nlr.ret_val);
        if (!(mp_obj_is_type(exception, &mp_type_KeyError) || mp_obj_is_type(exception, &mp_type_IndexError))) {
            
            
            usbd_pend_exception(exception);
        }
    }

    return NULL;
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false; 
}



static void runtime_dev_init(void) {
}

static void runtime_dev_reset(uint8_t rhport) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    if (!usbd) {
        return;
    }

    for (int epnum = 0; epnum < CFG_TUD_ENDPPOINT_MAX; epnum++) {
        for (int dir = 0; dir < 2; dir++) {
            usbd->xfer_data[epnum][dir] = mp_const_none;
        }
    }

    if (mp_obj_is_callable(usbd->reset_cb)) {
        usbd_callback_function_n(usbd->reset_cb, 0, NULL);
    }
}

















static uint8_t _runtime_dev_count_itfs(tusb_desc_interface_t const *itf_desc) {
    const tusb_desc_configuration_t *cfg_desc = (const void *)tud_descriptor_configuration_cb(0);
    const uint8_t *p_desc = (const void *)cfg_desc;
    const uint8_t *p_end = p_desc + cfg_desc->wTotalLength;
    assert(p_desc <= itf_desc && itf_desc < p_end);
    while (p_desc != (const void *)itf_desc && p_desc < p_end) {
        const uint8_t *next = tu_desc_next(p_desc);

        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE_ASSOCIATION
            && next == (const void *)itf_desc) {
            const tusb_desc_interface_assoc_t *desc_iad = (const void *)p_desc;
            return desc_iad->bInterfaceCount;
        }
        p_desc = next;
    }
    return 1; 
}









static uint16_t _runtime_dev_claim_itfs(tusb_desc_interface_t const *itf_desc, uint8_t assoc_itf_count, uint16_t max_len) {
    const uint8_t *p_desc = (const void *)itf_desc;
    const uint8_t *p_end = p_desc + max_len;
    while (p_desc < p_end) {
        if (tu_desc_type(p_desc) == TUSB_DESC_INTERFACE) {
            if (assoc_itf_count > 0) {
                
                assoc_itf_count--;
            } else {
                
                break;
            }
        } else if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
            
            if (tu_desc_type(p_desc) == TUSB_DESC_ENDPOINT) {
                bool r = usbd_edpt_open(USBD_RHPORT, (const void *)p_desc);
                if (!r) {
                    mp_obj_t exc = mp_obj_new_exception_arg1(&mp_type_OSError, MP_OBJ_NEW_SMALL_INT(MP_ENODEV));
                    usbd_pend_exception(exc);
                    break;
                }
            }
        }
        p_desc = tu_desc_next(p_desc);
    }
    return p_desc - (const uint8_t *)itf_desc;
}













static uint16_t runtime_dev_open(uint8_t rhport, tusb_desc_interface_t const *itf_desc, uint16_t max_len) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));

    
    if (!usbd) {
        return 0;
    }

    
    if (mp_usb_device_builtin_enabled(usbd) && itf_desc->bInterfaceNumber < USBD_ITF_BUILTIN_MAX) {
        return 0;
    }

    
    uint8_t assoc_itf_count = _runtime_dev_count_itfs(itf_desc);
    uint16_t claim_len = _runtime_dev_claim_itfs(itf_desc, assoc_itf_count, max_len);

    

    if (mp_obj_is_callable(usbd->open_itf_cb)) {
        
        usbd->control_data->items = (void *)itf_desc;
        usbd->control_data->len = claim_len;
        mp_obj_t args[] = { MP_OBJ_FROM_PTR(usbd->control_data) };
        usbd_callback_function_n(usbd->open_itf_cb, 1, args);
        usbd->control_data->len = 0;
        usbd->control_data->items = NULL;
    }

    return claim_len;
}

static bool runtime_dev_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    mp_obj_t cb_res = mp_const_false;
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    tusb_dir_t dir = request->bmRequestType_bit.direction;
    mp_buffer_info_t buf_info;
    bool result;

    if (!usbd) {
        return false;
    }

    if (mp_obj_is_callable(usbd->control_xfer_cb)) {
        usbd->control_data->items = (void *)request;
        usbd->control_data->len = sizeof(tusb_control_request_t);
        mp_obj_t args[] = {
            mp_obj_new_int(stage),
            MP_OBJ_FROM_PTR(usbd->control_data),
        };
        cb_res = usbd_callback_function_n(usbd->control_xfer_cb, MP_ARRAY_SIZE(args), args);
        usbd->control_data->items = NULL;
        usbd->control_data->len = 0;

        if (cb_res == MP_OBJ_NULL) {
            
            cb_res = mp_const_false;
        }
    }

    
    if (mp_get_buffer(cb_res, &buf_info, dir == TUSB_DIR_IN ? MP_BUFFER_READ : MP_BUFFER_RW)) {
        result = tud_control_xfer(USBD_RHPORT,
            request,
            buf_info.buf,
            buf_info.len);

        if (result) {
            
            usbd->xfer_data[0][dir] = cb_res;
        }
    } else {
        
        result = mp_obj_is_true(cb_res);

        if (stage == CONTROL_STAGE_SETUP && result) {
            
            
            tud_control_status(rhport, request);
        } else if (stage == CONTROL_STAGE_ACK) {
            
            usbd->xfer_data[0][dir] = mp_const_none;
        }
    }

    return result;
}

static bool runtime_dev_xfer_cb(uint8_t rhport, uint8_t ep_addr, xfer_result_t result, uint32_t xferred_bytes) {
    mp_obj_t ep = mp_obj_new_int(ep_addr);
    mp_obj_t cb_res = mp_const_false;
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    if (!usbd) {
        return false;
    }

    if (mp_obj_is_callable(usbd->xfer_cb)) {
        mp_obj_t args[] = {
            ep,
            MP_OBJ_NEW_SMALL_INT(result),
            MP_OBJ_NEW_SMALL_INT(xferred_bytes),
        };
        cb_res = usbd_callback_function_n(usbd->xfer_cb, MP_ARRAY_SIZE(args), args);
    }

    
    usbd->xfer_data[tu_edpt_number(ep_addr)][tu_edpt_dir(ep_addr)] = mp_const_none;

    return cb_res != MP_OBJ_NULL && mp_obj_is_true(cb_res);
}

static usbd_class_driver_t const _runtime_dev_driver =
{
    #if CFG_TUSB_DEBUG >= 2
    .name = "runtime_dev",
    #endif
    .init = runtime_dev_init,
    .reset = runtime_dev_reset,
    .open = runtime_dev_open,
    .control_xfer_cb = runtime_dev_control_xfer_cb,
    .xfer_cb = runtime_dev_xfer_cb,
    .sof = NULL
};

usbd_class_driver_t const *usbd_app_driver_get_cb(uint8_t *driver_count) {
    *driver_count = 1;
    return &_runtime_dev_driver;
}























void mp_usbd_init(void) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    bool need_usb;

    if (usbd == NULL) {
        
        #if CFG_TUD_CDC || CFG_TUD_MSC
        
        need_usb = true;
        #else
        
        need_usb = false;
        #endif
    } else {
        
        need_usb = usbd->active;
    }

    if (need_usb) {
        tusb_init(); 
        tud_connect(); 
    }
}






void mp_usbd_deinit(void) {
    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));
    MP_STATE_VM(usbd) = MP_OBJ_NULL;
    if (usbd && usbd->active) {
        
        mp_usbd_disconnect(usbd);
    }
}



static void mp_usbd_disconnect(mp_obj_usb_device_t *usbd) {
    if (!tusb_inited()) {
        return; 
    }

    if (usbd) {
        
        
        
        
        for (int epnum = 0; epnum < CFG_TUD_ENDPPOINT_MAX; epnum++) {
            for (int dir = 0; dir < 2; dir++) {
                if (usbd->xfer_data[epnum][dir] != mp_const_none) {
                    usbd_edpt_stall(USBD_RHPORT, tu_edpt_addr(epnum, dir));
                    usbd->xfer_data[epnum][dir] = mp_const_none;
                }
            }
        }
    }

    #if MICROPY_HW_USB_CDC
    
    tud_cdc_write_clear();
    
    usbd_edpt_stall(USBD_RHPORT, USBD_CDC_EP_IN);
    #endif

    bool was_connected = tud_connected();
    tud_disconnect();
    if (was_connected) {
        
        
        
        mp_hal_delay_ms(50);
    }
}


void mp_usbd_task_callback(mp_sched_node_t *node) {
    if (tud_inited() && !in_usbd_task) {
        mp_usbd_task_inner();
    }
    
    
    
    
    
}



void mp_usbd_task(void) {
    if (in_usbd_task) {
        
        
        
        mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("TinyUSB callback can't recurse"));
    }

    mp_usbd_task_inner();
}

static void mp_usbd_task_inner(void) {
    in_usbd_task = true;

    tud_task_ext(0, false);

    mp_obj_usb_device_t *usbd = MP_OBJ_TO_PTR(MP_STATE_VM(usbd));

    
    if (usbd && usbd->trigger) {
        if (usbd->active) {
            if (tud_connected()) {
                
                
                
                
                mp_usbd_disconnect(usbd);
            }
            tud_connect();
        } else {
            mp_usbd_disconnect(usbd);
        }
        usbd->trigger = false;
    }

    in_usbd_task = false;

    if (usbd) {
        
        

        
        
        mp_uint_t num_pend_excs = usbd->num_pend_excs;
        mp_obj_t pend_excs[MP_USBD_MAX_PEND_EXCS];
        for (mp_uint_t i = 0; i < MIN(MP_USBD_MAX_PEND_EXCS, num_pend_excs); i++) {
            pend_excs[i] = usbd->pend_excs[i];
            usbd->pend_excs[i] = mp_const_none;
        }
        usbd->num_pend_excs = 0;

        
        for (mp_uint_t i = 0; i < MIN(MP_USBD_MAX_PEND_EXCS, num_pend_excs); i++) {
            mp_obj_print_exception(&mp_plat_print, pend_excs[i]);
        }
        if (num_pend_excs > MP_USBD_MAX_PEND_EXCS) {
            mp_printf(&mp_plat_print, "%u additional exceptions in USB callbacks\n",
                num_pend_excs - MP_USBD_MAX_PEND_EXCS);
        }
    }
}

#endif 
