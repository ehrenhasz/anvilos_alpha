#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H
typedef struct {
	volatile unsigned int slock;
} arch_spinlock_t;
#define __ARCH_SPIN_LOCK_UNLOCKED__	0
#define __ARCH_SPIN_LOCK_LOCKED__	1
#define __ARCH_SPIN_LOCK_UNLOCKED	{ __ARCH_SPIN_LOCK_UNLOCKED__ }
#define __ARCH_SPIN_LOCK_LOCKED		{ __ARCH_SPIN_LOCK_LOCKED__ }
typedef struct {
	volatile unsigned int	counter;
#ifndef CONFIG_ARC_HAS_LLSC
	arch_spinlock_t		lock_mutex;
#endif
} arch_rwlock_t;
#define __ARCH_RW_LOCK_UNLOCKED__	0x01000000
#define __ARCH_RW_LOCK_UNLOCKED		{ .counter = __ARCH_RW_LOCK_UNLOCKED__ }
#endif
