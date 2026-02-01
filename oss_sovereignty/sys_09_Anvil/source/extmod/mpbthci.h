 

#ifndef MICROPY_INCLUDED_EXTMOD_MPBTHCI_H
#define MICROPY_INCLUDED_EXTMOD_MPBTHCI_H

#define MICROPY_PY_BLUETOOTH_HCI_READ_MODE_BYTE  (0)
#define MICROPY_PY_BLUETOOTH_HCI_READ_MODE_PACKET  (1)





int mp_bluetooth_hci_controller_init(void);
int mp_bluetooth_hci_controller_deinit(void);


int mp_bluetooth_hci_controller_sleep_maybe(void);

bool mp_bluetooth_hci_controller_woken(void);

int mp_bluetooth_hci_controller_wakeup(void);


int mp_bluetooth_hci_uart_init(uint32_t port, uint32_t baudrate);
int mp_bluetooth_hci_uart_deinit(void);
int mp_bluetooth_hci_uart_set_baudrate(uint32_t baudrate);
int mp_bluetooth_hci_uart_any(void);
int mp_bluetooth_hci_uart_write(const uint8_t *buf, size_t len);


int mp_bluetooth_hci_uart_readchar(void);


typedef void (*mp_bluetooth_hci_uart_readchar_t)(uint8_t chr);
int mp_bluetooth_hci_uart_readpacket(mp_bluetooth_hci_uart_readchar_t handler);

#endif 
