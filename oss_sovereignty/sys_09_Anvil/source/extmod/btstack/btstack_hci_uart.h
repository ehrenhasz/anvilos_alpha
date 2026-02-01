 

#ifndef MICROPY_INCLUDED_EXTMOD_BTSTACK_HCI_UART_H
#define MICROPY_INCLUDED_EXTMOD_BTSTACK_HCI_UART_H

#include "lib/btstack/src/btstack_uart_block.h"


extern const btstack_uart_block_t mp_bluetooth_btstack_hci_uart_block;


void mp_bluetooth_btstack_hci_uart_process(void);

#endif 
