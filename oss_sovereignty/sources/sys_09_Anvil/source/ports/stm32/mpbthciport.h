
#ifndef MICROPY_INCLUDED_STM32_MPBTHCIPORT_H
#define MICROPY_INCLUDED_STM32_MPBTHCIPORT_H

#include "boardctrl.h"


void mp_bluetooth_hci_init(void);


void mp_bluetooth_hci_poll_now_default(void);
void mp_bluetooth_hci_poll_in_ms_default(uint32_t ms);


static inline void mp_bluetooth_hci_poll_now(void) {
    MICROPY_BOARD_BT_HCI_POLL_NOW();
}


static inline void mp_bluetooth_hci_poll_in_ms(uint32_t ms) {
    MICROPY_BOARD_BT_HCI_POLL_IN_MS(ms);
}





void mp_bluetooth_hci_poll(void);

#endif 
