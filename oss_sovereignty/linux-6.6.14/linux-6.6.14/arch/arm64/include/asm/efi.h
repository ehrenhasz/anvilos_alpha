#ifndef _ASM_EFI_H
#define _ASM_EFI_H
#include <asm/boot.h>
#include <asm/cpufeature.h>
#include <asm/fpsimd.h>
#include <asm/io.h>
#include <asm/memory.h>
#include <asm/mmu_context.h>
#include <asm/neon.h>
#include <asm/ptrace.h>
#include <asm/tlbflush.h>
#ifdef CONFIG_EFI
extern void efi_init(void);
bool efi_runtime_fixup_exception(struct pt_regs *regs, const char *msg);
#else
#define efi_init()
static inline
bool efi_runtime_fixup_exception(struct pt_regs *regs, const char *msg)
{
	return false;
}
#endif
int efi_create_mapping(struct mm_struct *mm, efi_memory_desc_t *md);
int efi_set_mapping_permissions(struct mm_struct *mm, efi_memory_desc_t *md,
				bool has_bti);
#undef arch_efi_call_virt
#define arch_efi_call_virt(p, f, args...)				\
	__efi_rt_asm_wrapper((p)->f, #f, args)
extern u64 *efi_rt_stack_top;
efi_status_t __efi_rt_asm_wrapper(void *, const char *, ...);
void arch_efi_call_virt_setup(void);
void arch_efi_call_virt_teardown(void);
#define current_in_efi()						\
	(!preemptible() && efi_rt_stack_top != NULL &&			\
	 on_task_stack(current, READ_ONCE(efi_rt_stack_top[-1]), 1))
#define ARCH_EFI_IRQ_FLAGS_MASK (PSR_D_BIT | PSR_A_BIT | PSR_I_BIT | PSR_F_BIT)
#define arch_efi_save_flags(state_flags)		\
	((void)((state_flags) = read_sysreg(daif)))
#define arch_efi_restore_flags(state_flags)	write_sysreg(state_flags, daif)
#define EFI_KIMG_ALIGN	\
	(SEGMENT_ALIGN > THREAD_ALIGN ? SEGMENT_ALIGN : THREAD_ALIGN)
static inline unsigned long efi_get_max_initrd_addr(unsigned long image_addr)
{
	return (image_addr & ~(SZ_1G - 1UL)) + (1UL << (VA_BITS_MIN - 1));
}
static inline unsigned long efi_get_kimg_min_align(void)
{
	extern bool efi_nokaslr;
	return efi_nokaslr ? MIN_KIMG_ALIGN : EFI_KIMG_ALIGN;
}
#define EFI_ALLOC_ALIGN		SZ_64K
#define EFI_ALLOC_LIMIT		((1UL << 48) - 1)
extern unsigned long primary_entry_offset(void);
static inline void efi_set_pgd(struct mm_struct *mm)
{
	__switch_mm(mm);
	if (system_uses_ttbr0_pan()) {
		if (mm != current->active_mm) {
			update_saved_ttbr0(current, mm);
			uaccess_ttbr0_enable();
			post_ttbr_update_workaround();
		} else {
			uaccess_ttbr0_disable();
			update_saved_ttbr0(current, current->active_mm);
		}
	}
}
void efi_virtmap_load(void);
void efi_virtmap_unload(void);
static inline void efi_capsule_flush_cache_range(void *addr, int size)
{
	dcache_clean_inval_poc((unsigned long)addr, (unsigned long)addr + size);
}
efi_status_t efi_handle_corrupted_x18(efi_status_t s, const char *f);
void efi_icache_sync(unsigned long start, unsigned long end);
#endif  
