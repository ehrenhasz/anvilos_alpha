#ifndef __ASM_QSPINLOCK_PARAVIRT_H
#define __ASM_QSPINLOCK_PARAVIRT_H
#include <asm/ibt.h>
void __lockfunc __pv_queued_spin_unlock_slowpath(struct qspinlock *lock, u8 locked);
#ifdef CONFIG_64BIT
__PV_CALLEE_SAVE_REGS_THUNK(__pv_queued_spin_unlock_slowpath, ".spinlock.text");
#define __pv_queued_spin_unlock	__pv_queued_spin_unlock
#define PV_UNLOCK_ASM							\
	FRAME_BEGIN							\
	"push  %rdx\n\t"						\
	"mov   $0x1,%eax\n\t"						\
	"xor   %edx,%edx\n\t"						\
	LOCK_PREFIX "cmpxchg %dl,(%rdi)\n\t"				\
	"cmp   $0x1,%al\n\t"						\
	"jne   .slowpath\n\t"						\
	"pop   %rdx\n\t"						\
	FRAME_END							\
	ASM_RET								\
	".slowpath:\n\t"						\
	"push   %rsi\n\t"						\
	"movzbl %al,%esi\n\t"						\
	"call __raw_callee_save___pv_queued_spin_unlock_slowpath\n\t"	\
	"pop    %rsi\n\t"						\
	"pop    %rdx\n\t"						\
	FRAME_END
DEFINE_PARAVIRT_ASM(__raw_callee_save___pv_queued_spin_unlock,
		    PV_UNLOCK_ASM, .spinlock.text);
#else  
extern void __lockfunc __pv_queued_spin_unlock(struct qspinlock *lock);
__PV_CALLEE_SAVE_REGS_THUNK(__pv_queued_spin_unlock, ".spinlock.text");
#endif  
#endif
