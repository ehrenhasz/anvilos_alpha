#ifndef __SP_PUBLIC_H_INCLUDED__
#define __SP_PUBLIC_H_INCLUDED__
#include <type_support.h>
#include "system_local.h"
typedef struct sp_state_s		sp_state_t;
typedef struct sp_stall_s		sp_stall_t;
void cnd_sp_irq_enable(
    const sp_ID_t		ID,
    const bool			cnd);
void sp_get_state(
    const sp_ID_t		ID,
    sp_state_t			*state,
    sp_stall_t			*stall);
STORAGE_CLASS_SP_H void sp_ctrl_store(
    const sp_ID_t		ID,
    const hrt_address	reg,
    const hrt_data		value);
STORAGE_CLASS_SP_H hrt_data sp_ctrl_load(
    const sp_ID_t		ID,
    const hrt_address	reg);
STORAGE_CLASS_SP_H bool sp_ctrl_getbit(
    const sp_ID_t		ID,
    const hrt_address	reg,
    const unsigned int	bit);
STORAGE_CLASS_SP_H void sp_ctrl_setbit(
    const sp_ID_t		ID,
    const hrt_address	reg,
    const unsigned int	bit);
STORAGE_CLASS_SP_H void sp_ctrl_clearbit(
    const sp_ID_t		ID,
    const hrt_address	reg,
    const unsigned int	bit);
STORAGE_CLASS_SP_H void sp_dmem_store(
    const sp_ID_t		ID,
    hrt_address		addr,
    const void			*data,
    const size_t		size);
STORAGE_CLASS_SP_H void sp_dmem_load(
    const sp_ID_t		ID,
    const hrt_address	addr,
    void			*data,
    const size_t		size);
STORAGE_CLASS_SP_H void sp_dmem_store_uint8(
    const sp_ID_t		ID,
    hrt_address		addr,
    const uint8_t		data);
STORAGE_CLASS_SP_H void sp_dmem_store_uint16(
    const sp_ID_t		ID,
    hrt_address		addr,
    const uint16_t		data);
STORAGE_CLASS_SP_H void sp_dmem_store_uint32(
    const sp_ID_t		ID,
    hrt_address		addr,
    const uint32_t		data);
STORAGE_CLASS_SP_H uint8_t sp_dmem_load_uint8(
    const sp_ID_t		ID,
    const hrt_address	addr);
STORAGE_CLASS_SP_H uint16_t sp_dmem_load_uint16(
    const sp_ID_t		ID,
    const hrt_address	addr);
STORAGE_CLASS_SP_H uint32_t sp_dmem_load_uint32(
    const sp_ID_t		ID,
    const hrt_address	addr);
#endif  
