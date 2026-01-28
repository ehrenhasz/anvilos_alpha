#ifndef _ASM_SPINLOCK_H
#define _ASM_SPINLOCK_H
#include <asm/processor.h>
#include <asm-generic/qspinlock_types.h>
#define	queued_spin_unlock queued_spin_unlock
static inline void queued_spin_unlock(struct qspinlock *lock)
{
	mmiowb();
	smp_store_release(&lock->locked, 0);
}
#include <asm/qspinlock.h>
#include <asm/qrwlock.h>
#endif  
