 
#ifndef _ASM_X86_PGTABLE_3LEVEL_H
#define _ASM_X86_PGTABLE_3LEVEL_H

 

#define pte_ERROR(e)							\
	pr_err("%s:%d: bad pte %p(%08lx%08lx)\n",			\
	       __FILE__, __LINE__, &(e), (e).pte_high, (e).pte_low)
#define pmd_ERROR(e)							\
	pr_err("%s:%d: bad pmd %p(%016Lx)\n",				\
	       __FILE__, __LINE__, &(e), pmd_val(e))
#define pgd_ERROR(e)							\
	pr_err("%s:%d: bad pgd %p(%016Lx)\n",				\
	       __FILE__, __LINE__, &(e), pgd_val(e))

#define pxx_xchg64(_pxx, _ptr, _val) ({					\
	_pxx##val_t *_p = (_pxx##val_t *)_ptr;				\
	_pxx##val_t _o = *_p;						\
	do { } while (!try_cmpxchg64(_p, &_o, (_val)));			\
	native_make_##_pxx(_o);						\
})

 
static inline void native_set_pte(pte_t *ptep, pte_t pte)
{
	WRITE_ONCE(ptep->pte_high, pte.pte_high);
	smp_wmb();
	WRITE_ONCE(ptep->pte_low, pte.pte_low);
}

static inline void native_set_pte_atomic(pte_t *ptep, pte_t pte)
{
	pxx_xchg64(pte, ptep, native_pte_val(pte));
}

static inline void native_set_pmd(pmd_t *pmdp, pmd_t pmd)
{
	pxx_xchg64(pmd, pmdp, native_pmd_val(pmd));
}

static inline void native_set_pud(pud_t *pudp, pud_t pud)
{
#ifdef CONFIG_PAGE_TABLE_ISOLATION
	pud.p4d.pgd = pti_set_user_pgtbl(&pudp->p4d.pgd, pud.p4d.pgd);
#endif
	pxx_xchg64(pud, pudp, native_pud_val(pud));
}

 
static inline void native_pte_clear(struct mm_struct *mm, unsigned long addr,
				    pte_t *ptep)
{
	WRITE_ONCE(ptep->pte_low, 0);
	smp_wmb();
	WRITE_ONCE(ptep->pte_high, 0);
}

static inline void native_pmd_clear(pmd_t *pmdp)
{
	WRITE_ONCE(pmdp->pmd_low, 0);
	smp_wmb();
	WRITE_ONCE(pmdp->pmd_high, 0);
}

static inline void native_pud_clear(pud_t *pudp)
{
}

static inline void pud_clear(pud_t *pudp)
{
	set_pud(pudp, __pud(0));

	 
}


#ifdef CONFIG_SMP
static inline pte_t native_ptep_get_and_clear(pte_t *ptep)
{
	return pxx_xchg64(pte, ptep, 0ULL);
}

static inline pmd_t native_pmdp_get_and_clear(pmd_t *pmdp)
{
	return pxx_xchg64(pmd, pmdp, 0ULL);
}

static inline pud_t native_pudp_get_and_clear(pud_t *pudp)
{
	return pxx_xchg64(pud, pudp, 0ULL);
}
#else
#define native_ptep_get_and_clear(xp) native_local_ptep_get_and_clear(xp)
#define native_pmdp_get_and_clear(xp) native_local_pmdp_get_and_clear(xp)
#define native_pudp_get_and_clear(xp) native_local_pudp_get_and_clear(xp)
#endif

#ifndef pmdp_establish
#define pmdp_establish pmdp_establish
static inline pmd_t pmdp_establish(struct vm_area_struct *vma,
		unsigned long address, pmd_t *pmdp, pmd_t pmd)
{
	pmd_t old;

	 
	if (!(pmd_val(pmd) & _PAGE_PRESENT)) {
		 
		old.pmd_low = xchg(&pmdp->pmd_low, pmd.pmd_low);
		old.pmd_high = READ_ONCE(pmdp->pmd_high);
		WRITE_ONCE(pmdp->pmd_high, pmd.pmd_high);

		return old;
	}

	return pxx_xchg64(pmd, pmdp, pmd.pmd);
}
#endif

 
#define SWP_TYPE_BITS		5
#define _SWP_TYPE_MASK ((1U << SWP_TYPE_BITS) - 1)

#define SWP_OFFSET_FIRST_BIT	(_PAGE_BIT_PROTNONE + 1)

 
#define SWP_OFFSET_SHIFT	(SWP_OFFSET_FIRST_BIT + SWP_TYPE_BITS)

#define MAX_SWAPFILES_CHECK() BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > SWP_TYPE_BITS)
#define __swp_type(x)			(((x).val) & _SWP_TYPE_MASK)
#define __swp_offset(x)			((x).val >> SWP_TYPE_BITS)
#define __swp_entry(type, offset)	((swp_entry_t){((type) & _SWP_TYPE_MASK) \
					| (offset) << SWP_TYPE_BITS})

 
#define __swp_pteval_entry(type, offset) ((pteval_t) { \
	(~(pteval_t)(offset) << SWP_OFFSET_SHIFT >> SWP_TYPE_BITS) \
	| ((pteval_t)(type) << (64 - SWP_TYPE_BITS)) })

#define __swp_entry_to_pte(x)	((pte_t){ .pte = \
		__swp_pteval_entry(__swp_type(x), __swp_offset(x)) })
 
#define __pteval_swp_type(x) ((unsigned long)((x).pte >> (64 - SWP_TYPE_BITS)))
#define __pteval_swp_offset(x) ((unsigned long)(~((x).pte) << SWP_TYPE_BITS >> SWP_OFFSET_SHIFT))

#define __pte_to_swp_entry(pte)	(__swp_entry(__pteval_swp_type(pte), \
					     __pteval_swp_offset(pte)))

 
#define _PAGE_SWP_EXCLUSIVE	_PAGE_PSE

#include <asm/pgtable-invert.h>

#endif  
