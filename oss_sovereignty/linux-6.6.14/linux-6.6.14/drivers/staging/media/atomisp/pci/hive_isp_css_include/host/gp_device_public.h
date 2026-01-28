#ifndef __GP_DEVICE_PUBLIC_H_INCLUDED__
#define __GP_DEVICE_PUBLIC_H_INCLUDED__
#include "system_local.h"
typedef struct gp_device_state_s		gp_device_state_t;
void gp_device_get_state(
    const gp_device_ID_t		ID,
    gp_device_state_t			*state);
STORAGE_CLASS_GP_DEVICE_H void gp_device_reg_store(
    const gp_device_ID_t	ID,
    const unsigned int		reg_addr,
    const hrt_data			value);
STORAGE_CLASS_GP_DEVICE_H hrt_data gp_device_reg_load(
    const gp_device_ID_t	ID,
    const hrt_address	reg_addr);
#endif  
