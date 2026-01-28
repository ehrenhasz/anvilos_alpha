#ifndef _ASM_POWERPC_PGTABLE_TYPES_H
#define _ASM_POWERPC_PGTABLE_TYPES_H
#if defined(__CHECKER__) || !defined(CONFIG_PPC32)
#define STRICT_MM_TYPECHECKS
#endif
#if defined(CONFIG_PPC_8xx) && defined(CONFIG_PPC_16K_PAGES)
typedef struct { pte_basic_t pte, pte1, pte2, pte3; } pte_t;
#elif defined(STRICT_MM_TYPECHECKS)
typedef struct { pte_basic_t pte; } pte_t;
#else
typedef pte_basic_t pte_t;
#endif
#if defined(STRICT_MM_TYPECHECKS) || \
    (defined(CONFIG_PPC_8xx) && defined(CONFIG_PPC_16K_PAGES))
#define __pte(x)	((pte_t) { (x) })
static inline pte_basic_t pte_val(pte_t x)
{
	return x.pte;
}
#else
#define __pte(x)	((pte_t)(x))
static inline pte_basic_t pte_val(pte_t x)
{
	return x;
}
#endif
#ifdef CONFIG_PPC64
typedef struct { unsigned long pmd; } pmd_t;
#define __pmd(x)	((pmd_t) { (x) })
static inline unsigned long pmd_val(pmd_t x)
{
	return x.pmd;
}
typedef struct { unsigned long pud; } pud_t;
#define __pud(x)	((pud_t) { (x) })
static inline unsigned long pud_val(pud_t x)
{
	return x.pud;
}
#endif  
typedef struct { unsigned long pgd; } pgd_t;
#define __pgd(x)	((pgd_t) { (x) })
static inline unsigned long pgd_val(pgd_t x)
{
	return x.pgd;
}
typedef struct { unsigned long pgprot; } pgprot_t;
#define pgprot_val(x)	((x).pgprot)
#define __pgprot(x)	((pgprot_t) { (x) })
#ifdef CONFIG_PPC_64K_PAGES
typedef struct { pte_t pte; unsigned long hidx; } real_pte_t;
#else
typedef struct { pte_t pte; } real_pte_t;
#endif
#ifdef CONFIG_PPC_BOOK3S_64
#include <asm/cmpxchg.h>
static inline bool pte_xchg(pte_t *ptep, pte_t old, pte_t new)
{
	unsigned long *p = (unsigned long *)ptep;
	return pte_val(old) == __cmpxchg_u64(p, pte_val(old), pte_val(new));
}
#endif
#ifdef CONFIG_ARCH_HAS_HUGEPD
typedef struct { unsigned long pd; } hugepd_t;
#define __hugepd(x) ((hugepd_t) { (x) })
static inline unsigned long hpd_val(hugepd_t x)
{
	return x.pd;
}
#endif
#endif  
