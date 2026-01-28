
#ifndef MICROPY_INCLUDED_MIMXRT_TICKS_H
#define MICROPY_INCLUDED_MIMXRT_TICKS_H

void ticks_init(void);
uint32_t ticks_us32(void);
uint64_t ticks_us64(void);
uint32_t ticks_ms32(void);
void ticks_delay_us64(uint64_t us);

#endif 
