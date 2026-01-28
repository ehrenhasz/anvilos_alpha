#ifndef __MIPS_ASM_SYNC_H__
#define __MIPS_ASM_SYNC_H__
#define __SYNC_none	-1
#define __SYNC_full	0x00
#define __SYNC_aq	__SYNC_full
#define __SYNC_rl	__SYNC_full
#define __SYNC_mb	__SYNC_full
#ifdef CONFIG_CPU_CAVIUM_OCTEON
# define __SYNC_rmb	__SYNC_none
# define __SYNC_wmb	0x04
#else
# define __SYNC_rmb	__SYNC_full
# define __SYNC_wmb	__SYNC_full
#endif
#define __SYNC_ginv	0x14
#define __SYNC_always	(1 << 0)
#ifdef CONFIG_WEAK_ORDERING
# define __SYNC_weak_ordering	(1 << 1)
#else
# define __SYNC_weak_ordering	0
#endif
#ifdef CONFIG_WEAK_REORDERING_BEYOND_LLSC
# define __SYNC_weak_llsc	(1 << 2)
#else
# define __SYNC_weak_llsc	0
#endif
#ifdef CONFIG_CPU_LOONGSON3_WORKAROUNDS
# define __SYNC_loongson3_war	(1 << 31)
#else
# define __SYNC_loongson3_war	0
#endif
#ifdef CONFIG_CPU_CAVIUM_OCTEON
# define __SYNC_rpt(type)	(1 - (type == __SYNC_wmb))
#else
# define __SYNC_rpt(type)	1
#endif
#ifdef CONFIG_CPU_HAS_SYNC
# define ____SYNC(_type, _reason, _else)			\
	.if	(( _type ) != -1) && ( _reason );		\
	.set	push;						\
	.set	MIPS_ISA_LEVEL_RAW;				\
	.rept	__SYNC_rpt(_type);				\
	sync	_type;						\
	.endr;							\
	.set	pop;						\
	.else;							\
	_else;							\
	.endif
#else
# define ____SYNC(_type, _reason, _else)
#endif
#ifdef __ASSEMBLY__
# define ___SYNC(type, reason, else)				\
	____SYNC(type, reason, else)
#else
# define ___SYNC(type, reason, else)				\
	__stringify(____SYNC(type, reason, else))
#endif
#define __SYNC(type, reason)					\
	___SYNC(__SYNC_##type, __SYNC_##reason, )
#define __SYNC_ELSE(type, reason, else)				\
	___SYNC(__SYNC_##type, __SYNC_##reason, else)
#endif  
