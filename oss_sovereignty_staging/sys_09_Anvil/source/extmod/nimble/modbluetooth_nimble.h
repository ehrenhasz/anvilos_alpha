 

#ifndef MICROPY_INCLUDED_EXTMOD_NIMBLE_MODBLUETOOTH_NIMBLE_H
#define MICROPY_INCLUDED_EXTMOD_NIMBLE_MODBLUETOOTH_NIMBLE_H

#include "extmod/modbluetooth.h"

#define MP_BLUETOOTH_NIMBLE_MAX_SERVICES (8)

typedef struct _mp_bluetooth_nimble_pending_characteristic_t {
    uint16_t value_handle;
    uint8_t properties;
    mp_obj_bluetooth_uuid_t uuid;
    uint8_t ready;
} mp_bluetooth_nimble_pending_characteristic_t;

typedef struct _mp_bluetooth_nimble_root_pointers_t {
    
    mp_gatts_db_t gatts_db;

    
    size_t n_services;
    struct ble_gatt_svc_def *services[MP_BLUETOOTH_NIMBLE_MAX_SERVICES];

    #if MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS
    
    struct _mp_bluetooth_nimble_l2cap_channel_t *l2cap_chan;
    bool l2cap_listening;
    #endif

    #if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT
    
    
    uint16_t char_disc_end_handle;
    const mp_obj_bluetooth_uuid_t *char_filter_uuid;
    mp_bluetooth_nimble_pending_characteristic_t pending_char_result;
    #endif
} mp_bluetooth_nimble_root_pointers_t;

enum {
    MP_BLUETOOTH_NIMBLE_BLE_STATE_OFF,
    MP_BLUETOOTH_NIMBLE_BLE_STATE_STARTING,
    MP_BLUETOOTH_NIMBLE_BLE_STATE_WAITING_FOR_SYNC,
    MP_BLUETOOTH_NIMBLE_BLE_STATE_ACTIVE,
    MP_BLUETOOTH_NIMBLE_BLE_STATE_STOPPING,
};

extern volatile int mp_bluetooth_nimble_ble_state;





void mp_bluetooth_nimble_port_hci_init(void);


void mp_bluetooth_nimble_port_hci_deinit(void);


void mp_bluetooth_nimble_port_start(void);


void mp_bluetooth_nimble_port_shutdown(void);


void mp_bluetooth_nimble_sent_hci_packet(void);


#endif 
