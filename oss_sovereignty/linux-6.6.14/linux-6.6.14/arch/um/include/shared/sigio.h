#ifndef __SIGIO_H__
#define __SIGIO_H__
extern int write_sigio_irq(int fd);
extern void sigio_lock(void);
extern void sigio_unlock(void);
#endif
