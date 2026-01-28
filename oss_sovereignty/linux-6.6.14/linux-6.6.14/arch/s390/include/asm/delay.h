#ifndef _S390_DELAY_H
#define _S390_DELAY_H
void __ndelay(unsigned long nsecs);
void __udelay(unsigned long usecs);
void __delay(unsigned long loops);
#define ndelay(n) __ndelay((unsigned long)(n))
#define udelay(n) __udelay((unsigned long)(n))
#define mdelay(n) __udelay((unsigned long)(n) * 1000)
#endif  
