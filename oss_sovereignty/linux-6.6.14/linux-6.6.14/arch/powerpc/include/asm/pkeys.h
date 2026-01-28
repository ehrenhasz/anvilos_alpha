#ifndef _ASM_POWERPC_KEYS_H
#define _ASM_POWERPC_KEYS_H
#include <linux/jump_label.h>
#include <asm/firmware.h>
extern int num_pkey;
extern u32 reserved_allocation_mask;  
#define ARCH_VM_PKEY_FLAGS (VM_PKEY_BIT0 | VM_PKEY_BIT1 | VM_PKEY_BIT2 | \
			    VM_PKEY_BIT3 | VM_PKEY_BIT4)
#define PKEY_DISABLE_EXECUTE   0x4
#define PKEY_ACCESS_MASK       (PKEY_DISABLE_ACCESS | \
				PKEY_DISABLE_WRITE  | \
				PKEY_DISABLE_EXECUTE)
#ifdef CONFIG_PPC_BOOK3S_64
#include <asm/book3s/64/pkeys.h>
#else
#error "Not supported"
#endif
static inline u64 pkey_to_vmflag_bits(u16 pkey)
{
	return (((u64)pkey << VM_PKEY_SHIFT) & ARCH_VM_PKEY_FLAGS);
}
static inline int vma_pkey(struct vm_area_struct *vma)
{
	if (!mmu_has_feature(MMU_FTR_PKEY))
		return 0;
	return (vma->vm_flags & ARCH_VM_PKEY_FLAGS) >> VM_PKEY_SHIFT;
}
static inline int arch_max_pkey(void)
{
	return num_pkey;
}
#define pkey_alloc_mask(pkey) (0x1 << pkey)
#define mm_pkey_allocation_map(mm) (mm->context.pkey_allocation_map)
#define __mm_pkey_allocated(mm, pkey) {	\
	mm_pkey_allocation_map(mm) |= pkey_alloc_mask(pkey); \
}
#define __mm_pkey_free(mm, pkey) {	\
	mm_pkey_allocation_map(mm) &= ~pkey_alloc_mask(pkey);	\
}
#define __mm_pkey_is_allocated(mm, pkey)	\
	(mm_pkey_allocation_map(mm) & pkey_alloc_mask(pkey))
#define __mm_pkey_is_reserved(pkey) (reserved_allocation_mask & \
				       pkey_alloc_mask(pkey))
static inline bool mm_pkey_is_allocated(struct mm_struct *mm, int pkey)
{
	if (pkey < 0 || pkey >= arch_max_pkey())
		return false;
	if (__mm_pkey_is_reserved(pkey))
		return false;
	return __mm_pkey_is_allocated(mm, pkey);
}
static inline int mm_pkey_alloc(struct mm_struct *mm)
{
	u32 all_pkeys_mask = (u32)(~(0x0));
	int ret;
	if (!mmu_has_feature(MMU_FTR_PKEY))
		return -1;
	if (mm_pkey_allocation_map(mm) == all_pkeys_mask)
		return -1;
	ret = ffz((u32)mm_pkey_allocation_map(mm));
	__mm_pkey_allocated(mm, ret);
	return ret;
}
static inline int mm_pkey_free(struct mm_struct *mm, int pkey)
{
	if (!mmu_has_feature(MMU_FTR_PKEY))
		return -1;
	if (!mm_pkey_is_allocated(mm, pkey))
		return -EINVAL;
	__mm_pkey_free(mm, pkey);
	return 0;
}
extern int execute_only_pkey(struct mm_struct *mm);
extern int __arch_override_mprotect_pkey(struct vm_area_struct *vma,
					 int prot, int pkey);
static inline int arch_override_mprotect_pkey(struct vm_area_struct *vma,
					      int prot, int pkey)
{
	if (!mmu_has_feature(MMU_FTR_PKEY))
		return 0;
	if (pkey != -1)
		return pkey;
	return __arch_override_mprotect_pkey(vma, prot, pkey);
}
extern int __arch_set_user_pkey_access(struct task_struct *tsk, int pkey,
				       unsigned long init_val);
static inline int arch_set_user_pkey_access(struct task_struct *tsk, int pkey,
					    unsigned long init_val)
{
	if (!mmu_has_feature(MMU_FTR_PKEY))
		return -EINVAL;
	if (pkey == 0)
		return init_val ? -EINVAL : 0;
	return __arch_set_user_pkey_access(tsk, pkey, init_val);
}
static inline bool arch_pkeys_enabled(void)
{
	return mmu_has_feature(MMU_FTR_PKEY);
}
extern void pkey_mm_init(struct mm_struct *mm);
#endif  
