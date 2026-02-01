 
 

#ifndef __ISYS_STREAM2MMIO_PUBLIC_H_INCLUDED__
#define __ISYS_STREAM2MMIO_PUBLIC_H_INCLUDED__

 
 
STORAGE_CLASS_STREAM2MMIO_H void stream2mmio_get_state(
    const stream2mmio_ID_t ID,
    stream2mmio_state_t *state);

 
STORAGE_CLASS_STREAM2MMIO_H void stream2mmio_get_sid_state(
    const stream2mmio_ID_t ID,
    const stream2mmio_sid_ID_t sid_id,
    stream2mmio_sid_state_t *state);
 

 
 
STORAGE_CLASS_STREAM2MMIO_H hrt_data stream2mmio_reg_load(
    const stream2mmio_ID_t ID,
    const stream2mmio_sid_ID_t sid_id,
    const uint32_t reg_idx);

 
STORAGE_CLASS_STREAM2MMIO_H void stream2mmio_print_sid_state(
    stream2mmio_sid_state_t	*state);
 
STORAGE_CLASS_STREAM2MMIO_H void stream2mmio_dump_state(
    const stream2mmio_ID_t ID,
    stream2mmio_state_t *state);
 
STORAGE_CLASS_STREAM2MMIO_H void stream2mmio_reg_store(
    const stream2mmio_ID_t ID,
    const hrt_address reg,
    const hrt_data value);
 

#endif  
