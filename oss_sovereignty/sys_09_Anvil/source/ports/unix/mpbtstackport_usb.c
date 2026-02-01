 

#include "py/mpconfig.h"

#if MICROPY_PY_BLUETOOTH && MICROPY_BLUETOOTH_BTSTACK && MICROPY_BLUETOOTH_BTSTACK_USB

#include <pthread.h>
#include <unistd.h>

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

#include "lib/btstack/src/btstack.h"
#include "lib/btstack/src/hci_transport_usb.h"
#include "lib/btstack/platform/embedded/btstack_run_loop_embedded.h"
#include "lib/btstack/platform/embedded/hal_cpu.h"
#include "lib/btstack/platform/embedded/hal_time_ms.h"

#include "extmod/btstack/modbluetooth_btstack.h"

#include "mpbtstackport.h"

#if !MICROPY_PY_THREAD
#error Unix btstack requires MICROPY_PY_THREAD
#endif

static const useconds_t USB_POLL_INTERVAL_US = 1000;

void mp_bluetooth_btstack_port_init_usb(void) {
    
    char *path = getenv("MICROPYBTUSB");
    if (path != NULL) {
        uint8_t usb_path[7] = {0};
        size_t usb_path_len = 0;

        while (usb_path_len < MP_ARRAY_SIZE(usb_path)) {
            char *delimiter;
            usb_path[usb_path_len++] = strtol(path, &delimiter, 16);
            if (!delimiter || (*delimiter != ':' && *delimiter != '-')) {
                break;
            }
            path = delimiter + 1;
        }

        hci_transport_usb_set_path(usb_path_len, usb_path);
    }

    hci_init(hci_transport_usb_instance(), NULL);
}

static pthread_t bstack_thread_id;

void mp_bluetooth_btstack_port_deinit(void) {
    hci_power_control(HCI_POWER_OFF);

    
    pthread_join(bstack_thread_id, NULL);
}



extern bool mp_bluetooth_hci_poll(void);

static void *btstack_thread(void *arg) {
    (void)arg;
    hci_power_control(HCI_POWER_ON);

    
    
    
    
    

    while (true) {
        if (!mp_bluetooth_hci_poll()) {
            break;
        }

        
        
        usleep(USB_POLL_INTERVAL_US);
    }
    return NULL;
}

void mp_bluetooth_btstack_port_start(void) {
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&bstack_thread_id, &attr, &btstack_thread, NULL);
}

#endif 
