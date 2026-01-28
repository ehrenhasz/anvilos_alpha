

#ifndef __LINUX_MCS_SPINLOCK_H
#define __LINUX_MCS_SPINLOCK_H

#include <asm/mcs_spinlock.h>

struct mcs_spinlock {
	struct mcs_spinlock *next;
	int locked; 
	int count;  
};

#ifndef arch_mcs_spin_lock_contended

#define arch_mcs_spin_lock_contended(l)					\
do {									\
	smp_cond_load_acquire(l, VAL);					\
} while (0)
#endif

#ifndef arch_mcs_spin_unlock_contended

#define arch_mcs_spin_unlock_contended(l)				\
	smp_store_release((l), 1)
#endif




static inline
void mcs_spin_lock(struct mcs_spinlock **lock, struct mcs_spinlock *node)
{
	struct mcs_spinlock *prev;

	
	node->locked = 0;
	node->next   = NULL;

	
	prev = xchg(lock, node);
	if (likely(prev == NULL)) {
		
		return;
	}
	WRITE_ONCE(prev->next, node);

	
	arch_mcs_spin_lock_contended(&node->locked);
}


static inline
void mcs_spin_unlock(struct mcs_spinlock **lock, struct mcs_spinlock *node)
{
	struct mcs_spinlock *next = READ_ONCE(node->next);

	if (likely(!next)) {
		
		if (likely(cmpxchg_release(lock, node, NULL) == node))
			return;
		
		while (!(next = READ_ONCE(node->next)))
			cpu_relax();
	}

	
	arch_mcs_spin_unlock_contended(&next->locked);
}

#endif 
