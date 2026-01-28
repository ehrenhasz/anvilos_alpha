
#ifndef __SDRAM_H__
#define __SDRAM_H__
bool sdram_init(void);
void *sdram_start(void);
void *sdram_end(void);
void sdram_enter_low_power(void);
void sdram_leave_low_power(void);
void sdram_enter_power_down(void);
bool sdram_test(bool exhaustive);
#endif 
