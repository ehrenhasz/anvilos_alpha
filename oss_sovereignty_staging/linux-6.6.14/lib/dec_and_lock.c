
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>

 
int _atomic_dec_and_lock(atomic_t *atomic, spinlock_t *lock)
{
	 
	if (atomic_add_unless(atomic, -1, 1))
		return 0;

	 
	spin_lock(lock);
	if (atomic_dec_and_test(atomic))
		return 1;
	spin_unlock(lock);
	return 0;
}

EXPORT_SYMBOL(_atomic_dec_and_lock);

int _atomic_dec_and_lock_irqsave(atomic_t *atomic, spinlock_t *lock,
				 unsigned long *flags)
{
	 
	if (atomic_add_unless(atomic, -1, 1))
		return 0;

	 
	spin_lock_irqsave(lock, *flags);
	if (atomic_dec_and_test(atomic))
		return 1;
	spin_unlock_irqrestore(lock, *flags);
	return 0;
}
EXPORT_SYMBOL(_atomic_dec_and_lock_irqsave);

int _atomic_dec_and_raw_lock(atomic_t *atomic, raw_spinlock_t *lock)
{
	 
	if (atomic_add_unless(atomic, -1, 1))
		return 0;

	 
	raw_spin_lock(lock);
	if (atomic_dec_and_test(atomic))
		return 1;
	raw_spin_unlock(lock);
	return 0;
}
EXPORT_SYMBOL(_atomic_dec_and_raw_lock);

int _atomic_dec_and_raw_lock_irqsave(atomic_t *atomic, raw_spinlock_t *lock,
				     unsigned long *flags)
{
	 
	if (atomic_add_unless(atomic, -1, 1))
		return 0;

	 
	raw_spin_lock_irqsave(lock, *flags);
	if (atomic_dec_and_test(atomic))
		return 1;
	raw_spin_unlock_irqrestore(lock, *flags);
	return 0;
}
EXPORT_SYMBOL(_atomic_dec_and_raw_lock_irqsave);
