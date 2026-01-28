#ifndef _ASM_ARC_SERIAL_H
#define _ASM_ARC_SERIAL_H
extern unsigned int __init arc_early_base_baud(void);
#define BASE_BAUD	arc_early_base_baud()
#endif  
