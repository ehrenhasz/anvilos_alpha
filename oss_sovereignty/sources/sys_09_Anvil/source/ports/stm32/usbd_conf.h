



#ifndef MICROPY_INCLUDED_STM32_USBD_CONF_H
#define MICROPY_INCLUDED_STM32_USBD_CONF_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "py/mpconfig.h"

#define USBD_MAX_NUM_INTERFACES               8
#define USBD_MAX_NUM_CONFIGURATION            1
#define USBD_MAX_STR_DESC_SIZ                 0x100
#if MICROPY_HW_USB_SELF_POWERED
#define USBD_SELF_POWERED                     1
#else
#define USBD_SELF_POWERED                     0
#endif
#define USBD_DEBUG_LEVEL                      0


#ifndef USBD_ENABLE_VENDOR_DEVICE_REQUESTS
#define USBD_ENABLE_VENDOR_DEVICE_REQUESTS    (0)
#endif


#define USBD_PMA_RESERVE                      (64)
#define USBD_PMA_NUM_FIFO                     (16) 


#define USBD_FS_NUM_TX_FIFO                   (6)
#define USBD_FS_NUM_FIFO                      (1 + USBD_FS_NUM_TX_FIFO)
#define USBD_HS_NUM_TX_FIFO                   (9)
#define USBD_HS_NUM_FIFO                      (1 + USBD_HS_NUM_TX_FIFO)

#endif 


