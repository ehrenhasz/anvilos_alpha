#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H
#include <asm/spinlock_types.h>
#include <asm/processor.h>
#include <asm/barrier.h>
#define arch_spin_is_locked(x)	((x)->slock != __ARCH_SPIN_LOCK_UNLOCKED__)
#ifdef CONFIG_ARC_HAS_LLSC
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned int val;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[slock]]	\n"
	"	breq	%[val], %[LOCKED], 1b	\n"	 
	"	scond	%[LOCKED], [%[slock]]	\n"	 
	"	bnz	1b			\n"
	"					\n"
	: [val]		"=&r"	(val)
	: [slock]	"r"	(&(lock->slock)),
	  [LOCKED]	"r"	(__ARCH_SPIN_LOCK_LOCKED__)
	: "memory", "cc");
	smp_mb();
}
static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int val, got_it = 0;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[slock]]	\n"
	"	breq	%[val], %[LOCKED], 4f	\n"	 
	"	scond	%[LOCKED], [%[slock]]	\n"	 
	"	bnz	1b			\n"
	"	mov	%[got_it], 1		\n"
	"4:					\n"
	"					\n"
	: [val]		"=&r"	(val),
	  [got_it]	"+&r"	(got_it)
	: [slock]	"r"	(&(lock->slock)),
	  [LOCKED]	"r"	(__ARCH_SPIN_LOCK_LOCKED__)
	: "memory", "cc");
	smp_mb();
	return got_it;
}
static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_mb();
	WRITE_ONCE(lock->slock, __ARCH_SPIN_LOCK_UNLOCKED__);
}
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned int val;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[rwlock]]	\n"
	"	brls	%[val], %[WR_LOCKED], 1b\n"	 
	"	sub	%[val], %[val], 1	\n"	 
	"	scond	%[val], [%[rwlock]]	\n"
	"	bnz	1b			\n"
	"					\n"
	: [val]		"=&r"	(val)
	: [rwlock]	"r"	(&(rw->counter)),
	  [WR_LOCKED]	"ir"	(0)
	: "memory", "cc");
	smp_mb();
}
static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned int val, got_it = 0;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[rwlock]]	\n"
	"	brls	%[val], %[WR_LOCKED], 4f\n"	 
	"	sub	%[val], %[val], 1	\n"	 
	"	scond	%[val], [%[rwlock]]	\n"
	"	bnz	1b			\n"	 
	"	mov	%[got_it], 1		\n"
	"					\n"
	"4: ; --- done ---			\n"
	: [val]		"=&r"	(val),
	  [got_it]	"+&r"	(got_it)
	: [rwlock]	"r"	(&(rw->counter)),
	  [WR_LOCKED]	"ir"	(0)
	: "memory", "cc");
	smp_mb();
	return got_it;
}
static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned int val;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[rwlock]]	\n"
	"	brne	%[val], %[UNLOCKED], 1b	\n"	 
	"	mov	%[val], %[WR_LOCKED]	\n"
	"	scond	%[val], [%[rwlock]]	\n"
	"	bnz	1b			\n"
	"					\n"
	: [val]		"=&r"	(val)
	: [rwlock]	"r"	(&(rw->counter)),
	  [UNLOCKED]	"ir"	(__ARCH_RW_LOCK_UNLOCKED__),
	  [WR_LOCKED]	"ir"	(0)
	: "memory", "cc");
	smp_mb();
}
static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned int val, got_it = 0;
	__asm__ __volatile__(
	"1:	llock	%[val], [%[rwlock]]	\n"
	"	brne	%[val], %[UNLOCKED], 4f	\n"	 
	"	mov	%[val], %[WR_LOCKED]	\n"
	"	scond	%[val], [%[rwlock]]	\n"
	"	bnz	1b			\n"	 
	"	mov	%[got_it], 1		\n"
	"					\n"
	"4: ; --- done ---			\n"
	: [val]		"=&r"	(val),
	  [got_it]	"+&r"	(got_it)
	: [rwlock]	"r"	(&(rw->counter)),
	  [UNLOCKED]	"ir"	(__ARCH_RW_LOCK_UNLOCKED__),
	  [WR_LOCKED]	"ir"	(0)
	: "memory", "cc");
	smp_mb();
	return got_it;
}
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned int val;
	smp_mb();
	__asm__ __volatile__(
	"1:	llock	%[val], [%[rwlock]]	\n"
	"	add	%[val], %[val], 1	\n"
	"	scond	%[val], [%[rwlock]]	\n"
	"	bnz	1b			\n"
	"					\n"
	: [val]		"=&r"	(val)
	: [rwlock]	"r"	(&(rw->counter))
	: "memory", "cc");
}
static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	smp_mb();
	WRITE_ONCE(rw->counter, __ARCH_RW_LOCK_UNLOCKED__);
}
#else	 
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned int val = __ARCH_SPIN_LOCK_LOCKED__;
	smp_mb();
	__asm__ __volatile__(
	"1:	ex  %0, [%1]		\n"
	"	breq  %0, %2, 1b	\n"
	: "+&r" (val)
	: "r"(&(lock->slock)), "ir"(__ARCH_SPIN_LOCK_LOCKED__)
	: "memory");
	smp_mb();
}
static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned int val = __ARCH_SPIN_LOCK_LOCKED__;
	smp_mb();
	__asm__ __volatile__(
	"1:	ex  %0, [%1]		\n"
	: "+r" (val)
	: "r"(&(lock->slock))
	: "memory");
	smp_mb();
	return (val == __ARCH_SPIN_LOCK_UNLOCKED__);
}
static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	unsigned int val = __ARCH_SPIN_LOCK_UNLOCKED__;
	smp_mb();
	__asm__ __volatile__(
	"	ex  %0, [%1]		\n"
	: "+r" (val)
	: "r"(&(lock->slock))
	: "memory");
	smp_mb();
}
static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	int ret = 0;
	unsigned long flags;
	local_irq_save(flags);
	arch_spin_lock(&(rw->lock_mutex));
	if (rw->counter > 0) {
		rw->counter--;
		ret = 1;
	}
	arch_spin_unlock(&(rw->lock_mutex));
	local_irq_restore(flags);
	return ret;
}
static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	int ret = 0;
	unsigned long flags;
	local_irq_save(flags);
	arch_spin_lock(&(rw->lock_mutex));
	if (rw->counter == __ARCH_RW_LOCK_UNLOCKED__) {
		rw->counter = 0;
		ret = 1;
	}
	arch_spin_unlock(&(rw->lock_mutex));
	local_irq_restore(flags);
	return ret;
}
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	while (!arch_read_trylock(rw))
		cpu_relax();
}
static inline void arch_write_lock(arch_rwlock_t *rw)
{
	while (!arch_write_trylock(rw))
		cpu_relax();
}
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned long flags;
	local_irq_save(flags);
	arch_spin_lock(&(rw->lock_mutex));
	rw->counter++;
	arch_spin_unlock(&(rw->lock_mutex));
	local_irq_restore(flags);
}
static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	unsigned long flags;
	local_irq_save(flags);
	arch_spin_lock(&(rw->lock_mutex));
	rw->counter = __ARCH_RW_LOCK_UNLOCKED__;
	arch_spin_unlock(&(rw->lock_mutex));
	local_irq_restore(flags);
}
#endif
#endif  
