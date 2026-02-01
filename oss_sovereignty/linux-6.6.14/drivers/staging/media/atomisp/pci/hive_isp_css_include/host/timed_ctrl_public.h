 
 

#ifndef __TIMED_CTRL_PUBLIC_H_INCLUDED__
#define __TIMED_CTRL_PUBLIC_H_INCLUDED__

#include "system_local.h"

 
STORAGE_CLASS_TIMED_CTRL_H void timed_ctrl_reg_store(
    const timed_ctrl_ID_t	ID,
    const unsigned int		reg_addr,
    const hrt_data			value);

void timed_ctrl_snd_commnd(
    const timed_ctrl_ID_t				ID,
    hrt_data				mask,
    hrt_data				condition,
    hrt_data				counter,
    hrt_address				addr,
    hrt_data				value);

void timed_ctrl_snd_sp_commnd(
    const timed_ctrl_ID_t				ID,
    hrt_data				mask,
    hrt_data				condition,
    hrt_data				counter,
    const sp_ID_t			SP_ID,
    hrt_address				offset,
    hrt_data				value);

void timed_ctrl_snd_gpio_commnd(
    const timed_ctrl_ID_t				ID,
    hrt_data				mask,
    hrt_data				condition,
    hrt_data				counter,
    const gpio_ID_t			GPIO_ID,
    hrt_address				offset,
    hrt_data				value);

#endif  
