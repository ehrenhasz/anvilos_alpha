 

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_NIMBLE

#include "nimble/nimble_npl.h"

#include "extmod/nimble/modbluetooth_nimble.h"
#include "extmod/nimble/hal/hal_uart.h"

#define DEBUG_printf(...) 


bool mp_bluetooth_hci_poll(void) {
    

    if (mp_bluetooth_nimble_ble_state == MP_BLUETOOTH_NIMBLE_BLE_STATE_OFF) {
        DEBUG_printf("mp_bluetooth_hci_poll (unix nimble) -- shutdown\n");
        return false;
    }

    if (mp_bluetooth_nimble_ble_state >= MP_BLUETOOTH_NIMBLE_BLE_STATE_WAITING_FOR_SYNC) {
        
        mp_bluetooth_nimble_os_callout_process();

        
        mp_bluetooth_nimble_hci_uart_process(true);

        
        mp_bluetooth_nimble_os_eventq_run_all();
    }

    return true;
}

bool mp_bluetooth_hci_active(void) {
    return mp_bluetooth_nimble_ble_state != MP_BLUETOOTH_NIMBLE_BLE_STATE_OFF;
}


void mp_bluetooth_nimble_hci_uart_wfi(void) {
    
    
    mp_bluetooth_nimble_hci_uart_process(false);
}

#endif 
