 
 

#ifndef __PIXELGEN_PUBLIC_H_INCLUDED__
#define __PIXELGEN_PUBLIC_H_INCLUDED__

#ifdef ISP2401
 
 
STORAGE_CLASS_PIXELGEN_H void pixelgen_ctrl_get_state(
    const pixelgen_ID_t ID,
    pixelgen_ctrl_state_t *state);
 
STORAGE_CLASS_PIXELGEN_H void pixelgen_ctrl_dump_state(
    const pixelgen_ID_t ID,
    pixelgen_ctrl_state_t *state);
 

 
 
STORAGE_CLASS_PIXELGEN_H hrt_data pixelgen_ctrl_reg_load(
    const pixelgen_ID_t ID,
    const hrt_address reg);
 
STORAGE_CLASS_PIXELGEN_H void pixelgen_ctrl_reg_store(
    const pixelgen_ID_t ID,
    const hrt_address reg,
    const hrt_data value);
 

#endif  
#endif  
