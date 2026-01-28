

#ifndef MICROPY_INCLUDED_UNIX_BTSTACK_PORT_H
#define MICROPY_INCLUDED_UNIX_BTSTACK_PORT_H

#define MICROPY_HW_BLE_UART_ID (0)
#define MICROPY_HW_BLE_UART_BAUDRATE (1000000)

bool mp_bluetooth_hci_poll(void);

#if MICROPY_BLUETOOTH_BTSTACK_H4
void mp_bluetooth_hci_poll_h4(void);
void mp_bluetooth_btstack_port_init_h4(void);
#endif

#if MICROPY_BLUETOOTH_BTSTACK_USB
void mp_bluetooth_btstack_port_init_usb(void);
#endif

#endif 
