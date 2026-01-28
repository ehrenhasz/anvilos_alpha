#ifndef _ASM_POWERPC_SIMPLE_SPINLOCK_H
#define _ASM_POWERPC_SIMPLE_SPINLOCK_H
#include <linux/irqflags.h>
#include <linux/kcsan-checks.h>
#include <asm/paravirt.h>
#include <asm/paca.h>
#include <asm/synch.h>
#include <asm/ppc-opcode.h>
#ifdef CONFIG_PPC64
#ifdef __BIG_ENDIAN__
#define LOCK_TOKEN	(*(u32 *)(&get_paca()->lock_token))
#else
#define LOCK_TOKEN	(*(u32 *)(&get_paca()->paca_index))
#endif
#else
#define LOCK_TOKEN	1
#endif
static __always_inline int arch_spin_value_unlocked(arch_spinlock_t lock)
{
	return lock.slock == 0;
}
static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	return !arch_spin_value_unlocked(READ_ONCE(*lock));
}
static inline unsigned long __arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned long tmp, token;
	unsigned int eh = IS_ENABLED(CONFIG_PPC64);
	token = LOCK_TOKEN;
	__asm__ __volatile__(
"1:	lwarx		%0,0,%2,%[eh]\n\
	cmpwi		0,%0,0\n\
	bne-		2f\n\
	stwcx.		%1,0,%2\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"
	: "=&r" (tmp)
	: "r" (token), "r" (&lock->slock), [eh] "n" (eh)
	: "cr0", "memory");
	return tmp;
}
static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	return __arch_spin_trylock(lock) == 0;
}
#if defined(CONFIG_PPC_SPLPAR)
void splpar_spin_yield(arch_spinlock_t *lock);
void splpar_rw_yield(arch_rwlock_t *lock);
#else  
static inline void splpar_spin_yield(arch_spinlock_t *lock) {}
static inline void splpar_rw_yield(arch_rwlock_t *lock) {}
#endif
static inline void spin_yield(arch_spinlock_t *lock)
{
	if (is_shared_processor())
		splpar_spin_yield(lock);
	else
		barrier();
}
static inline void rw_yield(arch_rwlock_t *lock)
{
	if (is_shared_processor())
		splpar_rw_yield(lock);
	else
		barrier();
}
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	while (1) {
		if (likely(__arch_spin_trylock(lock) == 0))
			break;
		do {
			HMT_low();
			if (is_shared_processor())
				splpar_spin_yield(lock);
		} while (unlikely(lock->slock != 0));
		HMT_medium();
	}
}
static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	kcsan_mb();
	__asm__ __volatile__("# arch_spin_unlock\n\t"
				PPC_RELEASE_BARRIER: : :"memory");
	lock->slock = 0;
}
#ifdef CONFIG_PPC64
#define __DO_SIGN_EXTEND	"extsw	%0,%0\n"
#define WRLOCK_TOKEN		LOCK_TOKEN	 
#else
#define __DO_SIGN_EXTEND
#define WRLOCK_TOKEN		(-1)
#endif
static inline long __arch_read_trylock(arch_rwlock_t *rw)
{
	long tmp;
	unsigned int eh = IS_ENABLED(CONFIG_PPC64);
	__asm__ __volatile__(
"1:	lwarx		%0,0,%1,%[eh]\n"
	__DO_SIGN_EXTEND
"	addic.		%0,%0,1\n\
	ble-		2f\n"
"	stwcx.		%0,0,%1\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"	: "=&r" (tmp)
	: "r" (&rw->lock), [eh] "n" (eh)
	: "cr0", "xer", "memory");
	return tmp;
}
static inline long __arch_write_trylock(arch_rwlock_t *rw)
{
	long tmp, token;
	unsigned int eh = IS_ENABLED(CONFIG_PPC64);
	token = WRLOCK_TOKEN;
	__asm__ __volatile__(
"1:	lwarx		%0,0,%2,%[eh]\n\
	cmpwi		0,%0,0\n\
	bne-		2f\n"
"	stwcx.		%1,0,%2\n\
	bne-		1b\n"
	PPC_ACQUIRE_BARRIER
"2:"	: "=&r" (tmp)
	: "r" (token), "r" (&rw->lock), [eh] "n" (eh)
	: "cr0", "memory");
	return tmp;
}
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	while (1) {
		if (likely(__arch_read_trylock(rw) > 0))
			break;
		do {
			HMT_low();
			if (is_shared_processor())
				splpar_rw_yield(rw);
		} while (unlikely(rw->lock < 0));
		HMT_medium();
	}
}
static inline void arch_write_lock(arch_rwlock_t *rw)
{
	while (1) {
		if (likely(__arch_write_trylock(rw) == 0))
			break;
		do {
			HMT_low();
			if (is_shared_processor())
				splpar_rw_yield(rw);
		} while (unlikely(rw->lock != 0));
		HMT_medium();
	}
}
static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	return __arch_read_trylock(rw) > 0;
}
static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	return __arch_write_trylock(rw) == 0;
}
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	long tmp;
	__asm__ __volatile__(
	"# read_unlock\n\t"
	PPC_RELEASE_BARRIER
"1:	lwarx		%0,0,%1\n\
	addic		%0,%0,-1\n"
"	stwcx.		%0,0,%1\n\
	bne-		1b"
	: "=&r"(tmp)
	: "r"(&rw->lock)
	: "cr0", "xer", "memory");
}
static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	__asm__ __volatile__("# write_unlock\n\t"
				PPC_RELEASE_BARRIER: : :"memory");
	rw->lock = 0;
}
#define arch_spin_relax(lock)	spin_yield(lock)
#define arch_read_relax(lock)	rw_yield(lock)
#define arch_write_relax(lock)	rw_yield(lock)
#endif  
