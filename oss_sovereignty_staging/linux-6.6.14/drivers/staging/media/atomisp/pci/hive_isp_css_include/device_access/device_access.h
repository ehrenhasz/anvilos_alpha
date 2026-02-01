 
 

#ifndef __DEVICE_ACCESS_H_INCLUDED__
#define __DEVICE_ACCESS_H_INCLUDED__

 

#include <type_support.h>

 
#include "system_local.h"
 

 
typedef	hrt_address		sys_address;

 
void device_set_base_address(
    const sys_address		base_addr);

 
sys_address device_get_base_address(void);

 
uint8_t ia_css_device_load_uint8(
    const hrt_address		addr);

 
uint16_t ia_css_device_load_uint16(
    const hrt_address		addr);

 
uint32_t ia_css_device_load_uint32(
    const hrt_address		addr);

 
uint64_t ia_css_device_load_uint64(
    const hrt_address		addr);

 
void ia_css_device_store_uint8(
    const hrt_address		addr,
    const uint8_t			data);

 
void ia_css_device_store_uint16(
    const hrt_address		addr,
    const uint16_t			data);

 
void ia_css_device_store_uint32(
    const hrt_address		addr,
    const uint32_t			data);

 
void ia_css_device_store_uint64(
    const hrt_address		addr,
    const uint64_t			data);

 
void ia_css_device_load(
    const hrt_address		addr,
    void					*data,
    const size_t			size);

 
void ia_css_device_store(
    const hrt_address		addr,
    const void				*data,
    const size_t			size);

#endif  
