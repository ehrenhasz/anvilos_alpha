
#ifndef MICROPY_INCLUDED_MIMXRT_MPBTHCIPORT_H
#define MICROPY_INCLUDED_MIMXRT_MPBTHCIPORT_H


void mp_bluetooth_hci_init(void);


void mp_bluetooth_hci_poll_now(void);
void mp_bluetooth_hci_poll_in_ms(uint32_t ms);





void mp_bluetooth_hci_poll(void);

#endif 
