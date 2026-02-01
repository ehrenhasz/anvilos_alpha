 

#ifndef MICROPY_INCLUDED_EXTMOD_BTSTACK_MODBLUETOOTH_BTSTACK_H
#define MICROPY_INCLUDED_EXTMOD_BTSTACK_MODBLUETOOTH_BTSTACK_H

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_BTSTACK

#include "extmod/modbluetooth.h"

#include "lib/btstack/src/btstack.h"

typedef struct _mp_bluetooth_btstack_root_pointers_t {
    
    uint8_t *adv_data;
    
    size_t adv_data_alloc;

    
    mp_gatts_db_t gatts_db;

    #if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT
    
    gatt_client_notification_t notification;

    
    btstack_linked_list_t active_connections;
    #endif
} mp_bluetooth_btstack_root_pointers_t;

enum {
    MP_BLUETOOTH_BTSTACK_STATE_OFF,
    MP_BLUETOOTH_BTSTACK_STATE_STARTING,
    MP_BLUETOOTH_BTSTACK_STATE_ACTIVE,
    MP_BLUETOOTH_BTSTACK_STATE_HALTING,
    MP_BLUETOOTH_BTSTACK_STATE_TIMEOUT,
};

extern volatile int mp_bluetooth_btstack_state;

void mp_bluetooth_btstack_port_init(void);
void mp_bluetooth_btstack_port_deinit(void);
void mp_bluetooth_btstack_port_start(void);

#endif 

#endif 
