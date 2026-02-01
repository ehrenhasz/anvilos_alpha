 
 

#ifndef __FIFO_MONITOR_PUBLIC_H_INCLUDED__
#define __FIFO_MONITOR_PUBLIC_H_INCLUDED__

#include "system_local.h"

typedef struct fifo_channel_state_s		fifo_channel_state_t;
typedef struct fifo_switch_state_s		fifo_switch_state_t;
typedef struct fifo_monitor_state_s		fifo_monitor_state_t;

 
STORAGE_CLASS_FIFO_MONITOR_H void fifo_switch_set(
    const fifo_monitor_ID_t		ID,
    const fifo_switch_t			switch_id,
    const hrt_data				sel);

 
STORAGE_CLASS_FIFO_MONITOR_H hrt_data fifo_switch_get(
    const fifo_monitor_ID_t		ID,
    const fifo_switch_t			switch_id);

 
void fifo_monitor_get_state(
    const fifo_monitor_ID_t		ID,
    fifo_monitor_state_t		*state);

 
void fifo_channel_get_state(
    const fifo_monitor_ID_t		ID,
    const fifo_channel_t		channel_id,
    fifo_channel_state_t		*state);

 
void fifo_switch_get_state(
    const fifo_monitor_ID_t		ID,
    const fifo_switch_t			switch_id,
    fifo_switch_state_t			*state);

 
STORAGE_CLASS_FIFO_MONITOR_H void fifo_monitor_reg_store(
    const fifo_monitor_ID_t		ID,
    const unsigned int			reg,
    const hrt_data				value);

 
STORAGE_CLASS_FIFO_MONITOR_H hrt_data fifo_monitor_reg_load(
    const fifo_monitor_ID_t		ID,
    const unsigned int			reg);

#endif  
