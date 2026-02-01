 

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "py/objlist.h"
#include "py/runtime.h"
#include "py/mphal.h"

#if MICROPY_PY_NETWORK

#include "shared/netutils/netutils.h"
#include "extmod/modnetwork.h"

#if MICROPY_PY_NETWORK_CYW43

#include "lib/cyw43-driver/src/cyw43.h"
#endif

#ifdef MICROPY_PY_NETWORK_INCLUDEFILE
#include MICROPY_PY_NETWORK_INCLUDEFILE
#endif





char mod_network_country_code[2] = "XX";

#ifndef MICROPY_PY_NETWORK_HOSTNAME_DEFAULT
#error "MICROPY_PY_NETWORK_HOSTNAME_DEFAULT must be set in mpconfigport.h or mpconfigboard.h"
#endif

char mod_network_hostname_data[MICROPY_PY_NETWORK_HOSTNAME_MAX_LEN + 1] = MICROPY_PY_NETWORK_HOSTNAME_DEFAULT;

#ifdef MICROPY_PORT_NETWORK_INTERFACES

void mod_network_init(void) {
    mp_obj_list_init(&MP_STATE_PORT(mod_network_nic_list), 0);
}

void mod_network_deinit(void) {
    #if !MICROPY_PY_LWIP
    for (mp_uint_t i = 0; i < MP_STATE_PORT(mod_network_nic_list).len; i++) {
        mp_obj_t nic = MP_STATE_PORT(mod_network_nic_list).items[i];
        const mod_network_nic_protocol_t *nic_protocol = MP_OBJ_TYPE_GET_SLOT(mp_obj_get_type(nic), protocol);
        if (nic_protocol->deinit) {
            nic_protocol->deinit();
        }
    }
    #endif
}

void mod_network_register_nic(mp_obj_t nic) {
    for (mp_uint_t i = 0; i < MP_STATE_PORT(mod_network_nic_list).len; i++) {
        if (MP_STATE_PORT(mod_network_nic_list).items[i] == nic) {
            
            return;
        }
    }
    
    mp_obj_list_append(MP_OBJ_FROM_PTR(&MP_STATE_PORT(mod_network_nic_list)), nic);
}

mp_obj_t mod_network_find_nic(const uint8_t *ip) {
    
    for (mp_uint_t i = 0; i < MP_STATE_PORT(mod_network_nic_list).len; i++) {
        mp_obj_t nic = MP_STATE_PORT(mod_network_nic_list).items[i];
        
        
        return nic;
    }

    mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no available NIC"));
}

static mp_obj_t network_route(void) {
    return MP_OBJ_FROM_PTR(&MP_STATE_PORT(mod_network_nic_list));
}
static MP_DEFINE_CONST_FUN_OBJ_0(network_route_obj, network_route);

MP_REGISTER_ROOT_POINTER(mp_obj_list_t mod_network_nic_list);

#endif 

static mp_obj_t network_country(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return mp_obj_new_str(mod_network_country_code, 2);
    } else {
        size_t len;
        const char *str = mp_obj_str_get_data(args[0], &len);
        if (len != 2) {
            mp_raise_ValueError(NULL);
        }
        mod_network_country_code[0] = str[0];
        mod_network_country_code[1] = str[1];
        return mp_const_none;
    }
}

MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_network_country_obj, 0, 1, network_country);

mp_obj_t mod_network_hostname(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        return mp_obj_new_str(mod_network_hostname_data, strlen(mod_network_hostname_data));
    } else {
        size_t len;
        const char *str = mp_obj_str_get_data(args[0], &len);
        if (len > MICROPY_PY_NETWORK_HOSTNAME_MAX_LEN) {
            mp_raise_ValueError(NULL);
        }
        memcpy(mod_network_hostname_data, str, len);
        mod_network_hostname_data[len] = 0;
        return mp_const_none;
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_network_hostname_obj, 0, 1, mod_network_hostname);

#if LWIP_VERSION_MAJOR >= 2
MP_DEFINE_CONST_FUN_OBJ_KW(mod_network_ipconfig_obj, 0, mod_network_ipconfig);
#endif

static const mp_rom_map_elem_t mp_module_network_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_network) },
    { MP_ROM_QSTR(MP_QSTR_country), MP_ROM_PTR(&mod_network_country_obj) },
    { MP_ROM_QSTR(MP_QSTR_hostname), MP_ROM_PTR(&mod_network_hostname_obj) },
    #if LWIP_VERSION_MAJOR >= 2
    { MP_ROM_QSTR(MP_QSTR_ipconfig), MP_ROM_PTR(&mod_network_ipconfig_obj) },
    #endif

    
    #ifdef MICROPY_PORT_NETWORK_INTERFACES
    { MP_ROM_QSTR(MP_QSTR_route), MP_ROM_PTR(&network_route_obj) },
    MICROPY_PORT_NETWORK_INTERFACES
    #endif

    
    #ifdef MICROPY_PY_NETWORK_MODULE_GLOBALS_INCLUDEFILE
    #include MICROPY_PY_NETWORK_MODULE_GLOBALS_INCLUDEFILE
    #else
    
    { MP_ROM_QSTR(MP_QSTR_STA_IF), MP_ROM_INT(MOD_NETWORK_STA_IF) },
    { MP_ROM_QSTR(MP_QSTR_AP_IF), MP_ROM_INT(MOD_NETWORK_AP_IF) },
    #endif
};

static MP_DEFINE_CONST_DICT(mp_module_network_globals, mp_module_network_globals_table);

const mp_obj_module_t mp_module_network = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_network_globals,
};

MP_REGISTER_MODULE(MP_QSTR_network, mp_module_network);

#endif  
