#ifndef _ASM_PARISC_FUTEX_H
#define _ASM_PARISC_FUTEX_H
#include <linux/futex.h>
#include <linux/uaccess.h>
#include <asm/atomic.h>
#include <asm/errno.h>
static inline unsigned long _futex_hash_index(unsigned long ua)
{
	return (ua >> 2) & 0x3fc;
}
static inline void
_futex_spin_lock_irqsave(arch_spinlock_t *s, unsigned long *flags)
{
	local_irq_save(*flags);
	arch_spin_lock(s);
}
static inline void
_futex_spin_unlock_irqrestore(arch_spinlock_t *s, unsigned long *flags)
{
	arch_spin_unlock(s);
	local_irq_restore(*flags);
}
static inline int
arch_futex_atomic_op_inuser(int op, int oparg, int *oval, u32 __user *uaddr)
{
	extern u32 lws_lock_start[];
	unsigned long ua = (unsigned long)uaddr;
	arch_spinlock_t *s;
	unsigned long flags;
	int oldval, ret;
	u32 tmp;
	s = (arch_spinlock_t *)&lws_lock_start[_futex_hash_index(ua)];
	_futex_spin_lock_irqsave(s, &flags);
	if (unlikely(get_user(oldval, uaddr) != 0)) {
		ret = -EFAULT;
		goto out_pagefault_enable;
	}
	ret = 0;
	tmp = oldval;
	switch (op) {
	case FUTEX_OP_SET:
		tmp = oparg;
		break;
	case FUTEX_OP_ADD:
		tmp += oparg;
		break;
	case FUTEX_OP_OR:
		tmp |= oparg;
		break;
	case FUTEX_OP_ANDN:
		tmp &= ~oparg;
		break;
	case FUTEX_OP_XOR:
		tmp ^= oparg;
		break;
	default:
		ret = -ENOSYS;
		goto out_pagefault_enable;
	}
	if (unlikely(put_user(tmp, uaddr) != 0))
		ret = -EFAULT;
out_pagefault_enable:
	_futex_spin_unlock_irqrestore(s, &flags);
	if (!ret)
		*oval = oldval;
	return ret;
}
static inline int
futex_atomic_cmpxchg_inatomic(u32 *uval, u32 __user *uaddr,
			      u32 oldval, u32 newval)
{
	extern u32 lws_lock_start[];
	unsigned long ua = (unsigned long)uaddr;
	arch_spinlock_t *s;
	u32 val;
	unsigned long flags;
	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;
	s = (arch_spinlock_t *)&lws_lock_start[_futex_hash_index(ua)];
	_futex_spin_lock_irqsave(s, &flags);
	if (unlikely(get_user(val, uaddr) != 0)) {
		_futex_spin_unlock_irqrestore(s, &flags);
		return -EFAULT;
	}
	if (val == oldval && unlikely(put_user(newval, uaddr) != 0)) {
		_futex_spin_unlock_irqrestore(s, &flags);
		return -EFAULT;
	}
	*uval = val;
	_futex_spin_unlock_irqrestore(s, &flags);
	return 0;
}
#endif  
