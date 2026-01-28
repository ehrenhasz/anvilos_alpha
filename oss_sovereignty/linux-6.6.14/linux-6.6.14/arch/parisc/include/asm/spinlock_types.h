#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H
#define __ARCH_SPIN_LOCK_UNLOCKED_VAL	0x1a46
#define SPINLOCK_BREAK_INSN	0x0000c006	 
#ifndef __ASSEMBLY__
typedef struct {
	volatile unsigned int lock[4];
# define __ARCH_SPIN_LOCK_UNLOCKED	\
	{ { __ARCH_SPIN_LOCK_UNLOCKED_VAL, __ARCH_SPIN_LOCK_UNLOCKED_VAL, \
	    __ARCH_SPIN_LOCK_UNLOCKED_VAL, __ARCH_SPIN_LOCK_UNLOCKED_VAL } }
} arch_spinlock_t;
typedef struct {
	arch_spinlock_t		lock_mutex;
	volatile unsigned int	counter;
} arch_rwlock_t;
#endif  
#define __ARCH_RW_LOCK_UNLOCKED__       0x01000000
#define __ARCH_RW_LOCK_UNLOCKED         { .lock_mutex = __ARCH_SPIN_LOCK_UNLOCKED, \
					.counter = __ARCH_RW_LOCK_UNLOCKED__ }
#endif
