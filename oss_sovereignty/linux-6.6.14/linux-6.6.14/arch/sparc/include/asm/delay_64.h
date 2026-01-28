#ifndef _SPARC64_DELAY_H
#define _SPARC64_DELAY_H
#ifndef __ASSEMBLY__
void __delay(unsigned long loops);
void udelay(unsigned long usecs);
#define mdelay(n)	udelay((n) * 1000)
#endif  
#endif  
