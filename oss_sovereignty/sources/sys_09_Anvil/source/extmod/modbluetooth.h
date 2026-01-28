

#ifndef MICROPY_INCLUDED_EXTMOD_MODBLUETOOTH_H
#define MICROPY_INCLUDED_EXTMOD_MODBLUETOOTH_H

#include <stdbool.h>

#include "py/obj.h"
#include "py/objlist.h"
#include "py/ringbuf.h"


#ifndef MICROPY_PY_BLUETOOTH_RINGBUF_SIZE
#define MICROPY_PY_BLUETOOTH_RINGBUF_SIZE (128)
#endif

#ifndef MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE
#define MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE (0)
#endif

#ifndef MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT


#define MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT (MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE)
#endif

#ifndef MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS


#define MICROPY_PY_BLUETOOTH_USE_SYNC_EVENTS (0)
#endif


#ifndef MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS
#define MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS (0)
#endif



#ifndef MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING
#define MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING (0)
#endif



#ifndef MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD
#define MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD (0)
#endif



#ifndef MICROPY_PY_BLUETOOTH_ENTER
#define MICROPY_PY_BLUETOOTH_ENTER mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
#define MICROPY_PY_BLUETOOTH_EXIT MICROPY_END_ATOMIC_SECTION(atomic_state);
#endif


#ifndef MP_BLUETOOTH_DEFAULT_ATTR_LEN
#define MP_BLUETOOTH_DEFAULT_ATTR_LEN (20)
#endif

#define MP_BLUETOOTH_CCCD_LEN (2)


#define MP_BLUETOOTH_GAP_ADV_MAX_LEN (32)



#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_BROADCAST                  (0x0001)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ                       (0x0002)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_NO_RESPONSE          (0x0004)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE                      (0x0008)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_NOTIFY                     (0x0010)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_INDICATE                   (0x0020)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_AUTHENTICATED_SIGNED_WRITE (0x0040)






#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_AUX_WRITE                  (0x0100)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_ENCRYPTED             (0x0200)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_AUTHENTICATED         (0x0400)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_READ_AUTHORIZED            (0x0800)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_ENCRYPTED            (0x1000)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_AUTHENTICATED        (0x2000)
#define MP_BLUETOOTH_CHARACTERISTIC_FLAG_WRITE_AUTHORIZED           (0x4000)


#define MP_BLUETOOTH_GATTS_NO_ERROR                           (0x00)
#define MP_BLUETOOTH_GATTS_ERROR_READ_NOT_PERMITTED           (0x02)
#define MP_BLUETOOTH_GATTS_ERROR_WRITE_NOT_PERMITTED          (0x03)
#define MP_BLUETOOTH_GATTS_ERROR_INSUFFICIENT_AUTHENTICATION  (0x05)
#define MP_BLUETOOTH_GATTS_ERROR_INSUFFICIENT_AUTHORIZATION   (0x08)
#define MP_BLUETOOTH_GATTS_ERROR_INSUFFICIENT_ENCRYPTION      (0x0f)


#define MP_BLUETOOTH_WRITE_MODE_NO_RESPONSE     (0)
#define MP_BLUETOOTH_WRITE_MODE_WITH_RESPONSE   (1)


#define MP_BLUETOOTH_UUID_TYPE_16  (2)
#define MP_BLUETOOTH_UUID_TYPE_32  (4)
#define MP_BLUETOOTH_UUID_TYPE_128 (16)


#define MP_BLUETOOTH_IRQ_CENTRAL_CONNECT                (1)
#define MP_BLUETOOTH_IRQ_CENTRAL_DISCONNECT             (2)
#define MP_BLUETOOTH_IRQ_GATTS_WRITE                    (3)
#define MP_BLUETOOTH_IRQ_GATTS_READ_REQUEST             (4)
#define MP_BLUETOOTH_IRQ_SCAN_RESULT                    (5)
#define MP_BLUETOOTH_IRQ_SCAN_DONE                      (6)
#define MP_BLUETOOTH_IRQ_PERIPHERAL_CONNECT             (7)
#define MP_BLUETOOTH_IRQ_PERIPHERAL_DISCONNECT          (8)
#define MP_BLUETOOTH_IRQ_GATTC_SERVICE_RESULT           (9)
#define MP_BLUETOOTH_IRQ_GATTC_SERVICE_DONE             (10)
#define MP_BLUETOOTH_IRQ_GATTC_CHARACTERISTIC_RESULT    (11)
#define MP_BLUETOOTH_IRQ_GATTC_CHARACTERISTIC_DONE      (12)
#define MP_BLUETOOTH_IRQ_GATTC_DESCRIPTOR_RESULT        (13)
#define MP_BLUETOOTH_IRQ_GATTC_DESCRIPTOR_DONE          (14)
#define MP_BLUETOOTH_IRQ_GATTC_READ_RESULT              (15)
#define MP_BLUETOOTH_IRQ_GATTC_READ_DONE                (16)
#define MP_BLUETOOTH_IRQ_GATTC_WRITE_DONE               (17)
#define MP_BLUETOOTH_IRQ_GATTC_NOTIFY                   (18)
#define MP_BLUETOOTH_IRQ_GATTC_INDICATE                 (19)
#define MP_BLUETOOTH_IRQ_GATTS_INDICATE_DONE            (20)
#define MP_BLUETOOTH_IRQ_MTU_EXCHANGED                  (21)
#define MP_BLUETOOTH_IRQ_L2CAP_ACCEPT                   (22)
#define MP_BLUETOOTH_IRQ_L2CAP_CONNECT                  (23)
#define MP_BLUETOOTH_IRQ_L2CAP_DISCONNECT               (24)
#define MP_BLUETOOTH_IRQ_L2CAP_RECV                     (25)
#define MP_BLUETOOTH_IRQ_L2CAP_SEND_READY               (26)
#define MP_BLUETOOTH_IRQ_CONNECTION_UPDATE              (27)
#define MP_BLUETOOTH_IRQ_ENCRYPTION_UPDATE              (28)
#define MP_BLUETOOTH_IRQ_GET_SECRET                     (29)
#define MP_BLUETOOTH_IRQ_SET_SECRET                     (30)
#define MP_BLUETOOTH_IRQ_PASSKEY_ACTION                 (31)

#define MP_BLUETOOTH_ADDRESS_MODE_PUBLIC (0)
#define MP_BLUETOOTH_ADDRESS_MODE_RANDOM (1)
#define MP_BLUETOOTH_ADDRESS_MODE_RPA (2)
#define MP_BLUETOOTH_ADDRESS_MODE_NRPA (3)


#define MP_BLUETOOTH_IO_CAPABILITY_DISPLAY_ONLY        (0)
#define MP_BLUETOOTH_IO_CAPABILITY_DISPLAY_YESNO       (1)
#define MP_BLUETOOTH_IO_CAPABILITY_KEYBOARD_ONLY       (2)
#define MP_BLUETOOTH_IO_CAPABILITY_NO_INPUT_OUTPUT     (3)
#define MP_BLUETOOTH_IO_CAPABILITY_KEYBOARD_DISPLAY    (4)


#define MP_BLUETOOTH_PASSKEY_ACTION_NONE                (0)
#define MP_BLUETOOTH_PASSKEY_ACTION_INPUT               (2)
#define MP_BLUETOOTH_PASSKEY_ACTION_DISPLAY             (3)
#define MP_BLUETOOTH_PASSKEY_ACTION_NUMERIC_COMPARISON  (4)


#define MP_BLUETOOTH_PASSKEY_ACTION_NONE                (0)
#define MP_BLUETOOTH_PASSKEY_ACTION_INPUT               (2)
#define MP_BLUETOOTH_PASSKEY_ACTION_DISPLAY             (3)
#define MP_BLUETOOTH_PASSKEY_ACTION_NUMERIC_COMPARISON  (4)


#define MP_BLUETOOTH_GATTS_OP_NOTIFY                    (1)
#define MP_BLUETOOTH_GATTS_OP_INDICATE                  (2)









typedef struct {
    mp_obj_base_t base;
    uint8_t type;
    uint8_t data[16];
} mp_obj_bluetooth_uuid_t;

extern const mp_obj_type_t mp_type_bluetooth_uuid;

















int mp_bluetooth_init(void);


void mp_bluetooth_deinit(void);


bool mp_bluetooth_is_active(void);


void mp_bluetooth_get_current_address(uint8_t *addr_type, uint8_t *addr);


void mp_bluetooth_set_address_mode(uint8_t addr_mode);

#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

void mp_bluetooth_set_bonding(bool enabled);

void mp_bluetooth_set_mitm_protection(bool enabled);

void mp_bluetooth_set_le_secure(bool enabled);

void mp_bluetooth_set_io_capability(uint8_t capability);
#endif 


size_t mp_bluetooth_gap_get_device_name(const uint8_t **buf);
int mp_bluetooth_gap_set_device_name(const uint8_t *buf, size_t len);



int mp_bluetooth_gap_advertise_start(bool connectable, int32_t interval_us, const uint8_t *adv_data, size_t adv_data_len, const uint8_t *sr_data, size_t sr_data_len);


void mp_bluetooth_gap_advertise_stop(void);


int mp_bluetooth_gatts_register_service_begin(bool append);


int mp_bluetooth_gatts_register_service(mp_obj_bluetooth_uuid_t *service_uuid, mp_obj_bluetooth_uuid_t **characteristic_uuids, uint16_t *characteristic_flags, mp_obj_bluetooth_uuid_t **descriptor_uuids, uint16_t *descriptor_flags, uint8_t *num_descriptors, uint16_t *handles, size_t num_characteristics);

int mp_bluetooth_gatts_register_service_end(void);


int mp_bluetooth_gatts_read(uint16_t value_handle, const uint8_t **value, size_t *value_len);

int mp_bluetooth_gatts_write(uint16_t value_handle, const uint8_t *value, size_t value_len, bool send_update);

int mp_bluetooth_gatts_notify_indicate(uint16_t conn_handle, uint16_t value_handle, int gatts_op, const uint8_t *value, size_t value_len);



int mp_bluetooth_gatts_set_buffer(uint16_t value_handle, size_t len, bool append);


int mp_bluetooth_gap_disconnect(uint16_t conn_handle);


int mp_bluetooth_get_preferred_mtu(void);
int mp_bluetooth_set_preferred_mtu(uint16_t mtu);

#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

int mp_bluetooth_gap_pair(uint16_t conn_handle);


int mp_bluetooth_gap_passkey(uint16_t conn_handle, uint8_t action, mp_int_t passkey);
#endif 

#if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE

int mp_bluetooth_gap_scan_start(int32_t duration_ms, int32_t interval_us, int32_t window_us, bool active_scan);


int mp_bluetooth_gap_scan_stop(void);


int mp_bluetooth_gap_peripheral_connect(uint8_t addr_type, const uint8_t *addr, int32_t duration_ms, int32_t min_conn_interval_us, int32_t max_conn_interval_us);


int mp_bluetooth_gap_peripheral_connect_cancel(void);
#endif

#if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT


int mp_bluetooth_gattc_discover_primary_services(uint16_t conn_handle, const mp_obj_bluetooth_uuid_t *uuid);


int mp_bluetooth_gattc_discover_characteristics(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle, const mp_obj_bluetooth_uuid_t *uuid);


int mp_bluetooth_gattc_discover_descriptors(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle);


int mp_bluetooth_gattc_read(uint16_t conn_handle, uint16_t value_handle);


int mp_bluetooth_gattc_write(uint16_t conn_handle, uint16_t value_handle, const uint8_t *value, size_t value_len, unsigned int mode);


int mp_bluetooth_gattc_exchange_mtu(uint16_t conn_handle);
#endif 

#if MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS
int mp_bluetooth_l2cap_listen(uint16_t psm, uint16_t mtu);
int mp_bluetooth_l2cap_connect(uint16_t conn_handle, uint16_t psm, uint16_t mtu);
int mp_bluetooth_l2cap_disconnect(uint16_t conn_handle, uint16_t cid);
int mp_bluetooth_l2cap_send(uint16_t conn_handle, uint16_t cid, const uint8_t *buf, size_t len, bool *stalled);
int mp_bluetooth_l2cap_recvinto(uint16_t conn_handle, uint16_t cid, uint8_t *buf, size_t *len);
#endif 

#if MICROPY_PY_BLUETOOTH_ENABLE_HCI_CMD
int mp_bluetooth_hci_cmd(uint16_t ogf, uint16_t ocf, const uint8_t *req, size_t req_len, uint8_t *resp, size_t resp_len, uint8_t *status);
#endif 





void mp_bluetooth_gap_on_connected_disconnected(uint8_t event, uint16_t conn_handle, uint8_t addr_type, const uint8_t *addr);


void mp_bluetooth_gap_on_connection_update(uint16_t conn_handle, uint16_t conn_interval, uint16_t conn_latency, uint16_t supervision_timeout, uint16_t status);

#if MICROPY_PY_BLUETOOTH_ENABLE_PAIRING_BONDING

void mp_bluetooth_gatts_on_encryption_update(uint16_t conn_handle, bool encrypted, bool authenticated, bool bonded, uint8_t key_size);





bool mp_bluetooth_gap_on_get_secret(uint8_t type, uint8_t index, const uint8_t *key, uint16_t key_len, const uint8_t **value, size_t *value_len);
bool mp_bluetooth_gap_on_set_secret(uint8_t type, const uint8_t *key, size_t key_len, const uint8_t *value, size_t value_len);


void mp_bluetooth_gap_on_passkey_action(uint16_t conn_handle, uint8_t action, mp_int_t passkey);
#endif 


void mp_bluetooth_gatts_on_write(uint16_t conn_handle, uint16_t value_handle);


void mp_bluetooth_gatts_on_indicate_complete(uint16_t conn_handle, uint16_t value_handle, uint8_t status);



mp_int_t mp_bluetooth_gatts_on_read_request(uint16_t conn_handle, uint16_t value_handle);


void mp_bluetooth_gatts_on_mtu_exchanged(uint16_t conn_handle, uint16_t value);

#if MICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE

void mp_bluetooth_gap_on_scan_complete(void);


void mp_bluetooth_gap_on_scan_result(uint8_t addr_type, const uint8_t *addr, uint8_t adv_type, const int8_t rssi, const uint8_t *data, uint16_t data_len);
#endif 

#if MICROPY_PY_BLUETOOTH_ENABLE_GATT_CLIENT

void mp_bluetooth_gattc_on_primary_service_result(uint16_t conn_handle, uint16_t start_handle, uint16_t end_handle, mp_obj_bluetooth_uuid_t *service_uuid);


void mp_bluetooth_gattc_on_characteristic_result(uint16_t conn_handle, uint16_t value_handle, uint16_t end_handle, uint8_t properties, mp_obj_bluetooth_uuid_t *characteristic_uuid);


void mp_bluetooth_gattc_on_descriptor_result(uint16_t conn_handle, uint16_t handle, mp_obj_bluetooth_uuid_t *descriptor_uuid);


void mp_bluetooth_gattc_on_discover_complete(uint8_t event, uint16_t conn_handle, uint16_t status);


void mp_bluetooth_gattc_on_data_available(uint8_t event, uint16_t conn_handle, uint16_t value_handle, const uint8_t **data, uint16_t *data_len, size_t num);


void mp_bluetooth_gattc_on_read_write_status(uint8_t event, uint16_t conn_handle, uint16_t value_handle, uint16_t status);
#endif 

#if MICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS
mp_int_t mp_bluetooth_on_l2cap_accept(uint16_t conn_handle, uint16_t cid, uint16_t psm, uint16_t our_mtu, uint16_t peer_mtu);
void mp_bluetooth_on_l2cap_connect(uint16_t conn_handle, uint16_t cid, uint16_t psm, uint16_t our_mtu, uint16_t peer_mtu);
void mp_bluetooth_on_l2cap_disconnect(uint16_t conn_handle, uint16_t cid, uint16_t psm, uint16_t status);
void mp_bluetooth_on_l2cap_send_ready(uint16_t conn_handle, uint16_t cid, uint8_t status);
void mp_bluetooth_on_l2cap_recv(uint16_t conn_handle, uint16_t cid);
#endif 




typedef struct {
    
    uint8_t *data;
    
    size_t data_alloc;
    
    size_t data_len;
    
    bool append;
} mp_bluetooth_gatts_db_entry_t;

typedef mp_map_t *mp_gatts_db_t;

static inline void mp_bluetooth_gatts_db_create(mp_gatts_db_t *db) {
    *db = m_new(mp_map_t, 1);
}

static inline void mp_bluetooth_gatts_db_reset(mp_gatts_db_t db) {
    mp_map_init(db, 0);
}

void mp_bluetooth_gatts_db_create_entry(mp_gatts_db_t db, uint16_t handle, size_t len);
mp_bluetooth_gatts_db_entry_t *mp_bluetooth_gatts_db_lookup(mp_gatts_db_t db, uint16_t handle);
int mp_bluetooth_gatts_db_read(mp_gatts_db_t db, uint16_t handle, const uint8_t **value, size_t *value_len);
int mp_bluetooth_gatts_db_write(mp_gatts_db_t db, uint16_t handle, const uint8_t *value, size_t value_len);
int mp_bluetooth_gatts_db_resize(mp_gatts_db_t db, uint16_t handle, size_t len, bool append);

#endif 
