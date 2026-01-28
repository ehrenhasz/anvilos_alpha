#ifndef __ISP_PUBLIC_H_INCLUDED__
#define __ISP_PUBLIC_H_INCLUDED__
#include <type_support.h>
#include "system_local.h"
void cnd_isp_irq_enable(
    const isp_ID_t		ID,
    const bool			cnd);
void isp_get_state(
    const isp_ID_t		ID,
    isp_state_t			*state,
    isp_stall_t			*stall);
STORAGE_CLASS_ISP_H void isp_ctrl_store(
    const isp_ID_t		ID,
    const unsigned int	reg,
    const hrt_data		value);
STORAGE_CLASS_ISP_H hrt_data isp_ctrl_load(
    const isp_ID_t		ID,
    const unsigned int	reg);
STORAGE_CLASS_ISP_H bool isp_ctrl_getbit(
    const isp_ID_t		ID,
    const unsigned int	reg,
    const unsigned int	bit);
STORAGE_CLASS_ISP_H void isp_ctrl_setbit(
    const isp_ID_t		ID,
    const unsigned int	reg,
    const unsigned int	bit);
STORAGE_CLASS_ISP_H void isp_ctrl_clearbit(
    const isp_ID_t		ID,
    const unsigned int	reg,
    const unsigned int	bit);
STORAGE_CLASS_ISP_H void isp_dmem_store(
    const isp_ID_t		ID,
    unsigned int		addr,
    const void			*data,
    const size_t		size);
STORAGE_CLASS_ISP_H void isp_dmem_load(
    const isp_ID_t		ID,
    const unsigned int	addr,
    void				*data,
    const size_t		size);
STORAGE_CLASS_ISP_H void isp_dmem_store_uint32(
    const isp_ID_t		ID,
    unsigned int		addr,
    const uint32_t		data);
STORAGE_CLASS_ISP_H uint32_t isp_dmem_load_uint32(
    const isp_ID_t		ID,
    const unsigned int	addr);
STORAGE_CLASS_ISP_H uint32_t isp_2w_cat_1w(
    const u16		x0,
    const uint16_t		x1);
unsigned int isp_is_ready(isp_ID_t ID);
unsigned int isp_is_sleeping(isp_ID_t ID);
void isp_start(isp_ID_t ID);
void isp_wake(isp_ID_t ID);
#endif  
