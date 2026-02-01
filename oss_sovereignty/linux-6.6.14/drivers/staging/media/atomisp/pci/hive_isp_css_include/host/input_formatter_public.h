 
 

#ifndef __INPUT_FORMATTER_PUBLIC_H_INCLUDED__
#define __INPUT_FORMATTER_PUBLIC_H_INCLUDED__

#include <type_support.h>
#include "system_local.h"

 
void input_formatter_rst(
    const input_formatter_ID_t		ID);

 
void input_formatter_set_fifo_blocking_mode(
    const input_formatter_ID_t		ID,
    const bool						enable);

 
unsigned int input_formatter_get_alignment(
    const input_formatter_ID_t		ID);

 
void input_formatter_get_switch_state(
    const input_formatter_ID_t		ID,
    input_formatter_switch_state_t	*state);

 
void input_formatter_get_state(
    const input_formatter_ID_t		ID,
    input_formatter_state_t			*state);

 
void input_formatter_bin_get_state(
    const input_formatter_ID_t		ID,
    input_formatter_bin_state_t		*state);

 
STORAGE_CLASS_INPUT_FORMATTER_H void input_formatter_reg_store(
    const input_formatter_ID_t	ID,
    const hrt_address		reg_addr,
    const hrt_data				value);

 
STORAGE_CLASS_INPUT_FORMATTER_H hrt_data input_formatter_reg_load(
    const input_formatter_ID_t	ID,
    const unsigned int			reg_addr);

#endif  
