#ifndef _XTENSA_SPINLOCK_H
#define _XTENSA_SPINLOCK_H
#include <asm/barrier.h>
#include <asm/qspinlock.h>
#include <asm/qrwlock.h>
#define smp_mb__after_spinlock()	smp_mb()
#endif	 
