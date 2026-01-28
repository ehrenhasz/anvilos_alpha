
#ifndef MICROPY_INCLUDED_STM32_USBD_MSC_INTERFACE_H
#define MICROPY_INCLUDED_STM32_USBD_MSC_INTERFACE_H

extern const USBD_StorageTypeDef usbd_msc_fops;

void usbd_msc_init_lu(size_t lu_n, const void *lu_data);

#endif 
