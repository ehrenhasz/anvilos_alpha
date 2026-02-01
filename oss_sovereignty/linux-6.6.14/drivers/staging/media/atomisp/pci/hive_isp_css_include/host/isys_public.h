 
 

#ifndef __ISYS_PUBLIC_H_INCLUDED__
#define __ISYS_PUBLIC_H_INCLUDED__

#ifdef ISP2401
 
STORAGE_CLASS_INPUT_SYSTEM_H input_system_err_t input_system_get_state(
    const input_system_ID_t	ID,
    input_system_state_t *state);
 
STORAGE_CLASS_INPUT_SYSTEM_H void input_system_dump_state(
    const input_system_ID_t	ID,
    input_system_state_t *state);
#endif  
#endif  
