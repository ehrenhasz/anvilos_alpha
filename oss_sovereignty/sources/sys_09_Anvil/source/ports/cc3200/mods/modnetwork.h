
#ifndef MICROPY_INCLUDED_CC3200_MODS_MODNETWORK_H
#define MICROPY_INCLUDED_CC3200_MODS_MODNETWORK_H


#define MOD_NETWORK_IPV4ADDR_BUF_SIZE             (4)


typedef struct _mod_network_socket_base_t {
    union {
        struct {
            
            
            uint8_t domain;
            int8_t fileno;
            uint8_t type;
            uint8_t proto;
        } u_param;
        int16_t sd;
    };
    uint32_t timeout_ms; 
    bool cert_req;
} mod_network_socket_base_t;

typedef struct _mod_network_socket_obj_t {
    mp_obj_base_t base;
    mod_network_socket_base_t sock_base;
} mod_network_socket_obj_t;


extern const mp_obj_type_t mod_network_nic_type_wlan;


void mod_network_init0(void);

#endif 
