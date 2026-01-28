

#ifndef BLUETOOTH_LE_UART_H__
#define BLUETOOTH_LE_UART_H__

#if BLUETOOTH_SD

#include "modubluepy.h"
#include "ble_drv.h"

void ble_uart_init0(void);
void ble_uart_advertise(void);
bool ble_uart_connected(void);
bool ble_uart_enabled(void);

#endif 

#endif 
