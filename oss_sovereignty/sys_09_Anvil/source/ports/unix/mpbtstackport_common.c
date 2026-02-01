 

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_BTSTACK

#include "lib/btstack/src/btstack.h"

#include "lib/btstack/platform/embedded/btstack_run_loop_embedded.h"
#include "lib/btstack/platform/embedded/hal_cpu.h"
#include "lib/btstack/platform/embedded/hal_time_ms.h"
#include "lib/btstack/platform/embedded/hci_dump_embedded_stdout.h"

#include "extmod/btstack/modbluetooth_btstack.h"

#include "mpbtstackport.h"


bool mp_bluetooth_hci_poll(void) {
    if (mp_bluetooth_btstack_state == MP_BLUETOOTH_BTSTACK_STATE_STARTING || mp_bluetooth_btstack_state == MP_BLUETOOTH_BTSTACK_STATE_ACTIVE || mp_bluetooth_btstack_state == MP_BLUETOOTH_BTSTACK_STATE_HALTING) {
        
        mp_uint_t atomic_state = MICROPY_BEGIN_ATOMIC_SECTION();
        #if MICROPY_BLUETOOTH_BTSTACK_H4
        mp_bluetooth_hci_poll_h4();
        #endif
        btstack_run_loop_embedded_execute_once();
        MICROPY_END_ATOMIC_SECTION(atomic_state);

        return true;
    }

    return false;
}

bool mp_bluetooth_hci_active(void) {
    return mp_bluetooth_btstack_state != MP_BLUETOOTH_BTSTACK_STATE_OFF
           && mp_bluetooth_btstack_state != MP_BLUETOOTH_BTSTACK_STATE_TIMEOUT;
}




void hal_cpu_disable_irqs(void) {
}

void hal_cpu_enable_irqs(void) {
}

void hal_cpu_enable_irqs_and_sleep(void) {
}

uint32_t hal_time_ms(void) {
    return mp_hal_ticks_ms();
}

void mp_bluetooth_btstack_port_init(void) {
    btstack_run_loop_init(btstack_run_loop_embedded_get_instance());

    

    #if MICROPY_BLUETOOTH_BTSTACK_H4
    mp_bluetooth_btstack_port_init_h4();
    #endif

    #if MICROPY_BLUETOOTH_BTSTACK_USB
    mp_bluetooth_btstack_port_init_usb();
    #endif
}

#endif 
