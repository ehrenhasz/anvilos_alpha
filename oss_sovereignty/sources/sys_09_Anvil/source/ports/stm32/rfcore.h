
#ifndef MICROPY_INCLUDED_STM32_RFCORE_H
#define MICROPY_INCLUDED_STM32_RFCORE_H

#include <stdint.h>

typedef void (*rfcore_ble_msg_callback_t)(void *, const uint8_t *, size_t);

void rfcore_init(void);

void rfcore_ble_init(void);
bool rfcore_ble_reset(void);
void rfcore_ble_hci_cmd(size_t len, const uint8_t *src);
size_t rfcore_ble_check_msg(rfcore_ble_msg_callback_t cb, void *env);
void rfcore_ble_set_txpower(uint8_t level);

void rfcore_start_flash_erase(void);
void rfcore_end_flash_erase(void);

MP_DECLARE_CONST_FUN_OBJ_0(rfcore_status_obj);
MP_DECLARE_CONST_FUN_OBJ_1(rfcore_fw_version_obj);
MP_DECLARE_CONST_FUN_OBJ_VAR_BETWEEN(rfcore_sys_hci_obj);

#endif 
