#ifndef _ASM_RISCV_DELAY_H
#define _ASM_RISCV_DELAY_H
extern unsigned long riscv_timebase;
#define udelay udelay
extern void udelay(unsigned long usecs);
#define ndelay ndelay
extern void ndelay(unsigned long nsecs);
extern void __delay(unsigned long cycles);
#endif  
