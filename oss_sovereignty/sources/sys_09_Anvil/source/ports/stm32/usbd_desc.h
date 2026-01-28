
#ifndef MICROPY_INCLUDED_STM32_USBD_DESC_H
#define MICROPY_INCLUDED_STM32_USBD_DESC_H

#include "usbd_cdc_msc_hid.h"

extern const USBD_DescriptorsTypeDef USBD_Descriptors;

void USBD_SetVIDPIDRelease(usbd_cdc_msc_hid_state_t *usbd, uint16_t vid, uint16_t pid, uint16_t device_release_num, int cdc_only);

#endif 
