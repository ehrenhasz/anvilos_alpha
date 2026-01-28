#ifndef _ASM_PGTABLE_BITS_H
#define _ASM_PGTABLE_BITS_H
#define	_PAGE_VALID_SHIFT	0
#define	_PAGE_ACCESSED_SHIFT	0   
#define	_PAGE_DIRTY_SHIFT	1
#define	_PAGE_PLV_SHIFT		2   
#define	_CACHE_SHIFT		4   
#define	_PAGE_GLOBAL_SHIFT	6
#define	_PAGE_HUGE_SHIFT	6   
#define	_PAGE_PRESENT_SHIFT	7
#define	_PAGE_WRITE_SHIFT	8
#define	_PAGE_MODIFIED_SHIFT	9
#define	_PAGE_PROTNONE_SHIFT	10
#define	_PAGE_SPECIAL_SHIFT	11
#define	_PAGE_HGLOBAL_SHIFT	12  
#define	_PAGE_PFN_SHIFT		12
#define	_PAGE_SWP_EXCLUSIVE_SHIFT 23
#define	_PAGE_PFN_END_SHIFT	48
#define	_PAGE_PRESENT_INVALID_SHIFT 60
#define	_PAGE_NO_READ_SHIFT	61
#define	_PAGE_NO_EXEC_SHIFT	62
#define	_PAGE_RPLV_SHIFT	63
#define _PAGE_PRESENT		(_ULCAST_(1) << _PAGE_PRESENT_SHIFT)
#define _PAGE_PRESENT_INVALID	(_ULCAST_(1) << _PAGE_PRESENT_INVALID_SHIFT)
#define _PAGE_WRITE		(_ULCAST_(1) << _PAGE_WRITE_SHIFT)
#define _PAGE_ACCESSED		(_ULCAST_(1) << _PAGE_ACCESSED_SHIFT)
#define _PAGE_MODIFIED		(_ULCAST_(1) << _PAGE_MODIFIED_SHIFT)
#define _PAGE_PROTNONE		(_ULCAST_(1) << _PAGE_PROTNONE_SHIFT)
#define _PAGE_SPECIAL		(_ULCAST_(1) << _PAGE_SPECIAL_SHIFT)
#define _PAGE_SWP_EXCLUSIVE	(_ULCAST_(1) << _PAGE_SWP_EXCLUSIVE_SHIFT)
#define _PAGE_VALID		(_ULCAST_(1) << _PAGE_VALID_SHIFT)
#define _PAGE_DIRTY		(_ULCAST_(1) << _PAGE_DIRTY_SHIFT)
#define _PAGE_PLV		(_ULCAST_(3) << _PAGE_PLV_SHIFT)
#define _PAGE_GLOBAL		(_ULCAST_(1) << _PAGE_GLOBAL_SHIFT)
#define _PAGE_HUGE		(_ULCAST_(1) << _PAGE_HUGE_SHIFT)
#define _PAGE_HGLOBAL		(_ULCAST_(1) << _PAGE_HGLOBAL_SHIFT)
#define _PAGE_NO_READ		(_ULCAST_(1) << _PAGE_NO_READ_SHIFT)
#define _PAGE_NO_EXEC		(_ULCAST_(1) << _PAGE_NO_EXEC_SHIFT)
#define _PAGE_RPLV		(_ULCAST_(1) << _PAGE_RPLV_SHIFT)
#define _CACHE_MASK		(_ULCAST_(3) << _CACHE_SHIFT)
#define PFN_PTE_SHIFT		(PAGE_SHIFT - 12 + _PAGE_PFN_SHIFT)
#define _PAGE_USER	(PLV_USER << _PAGE_PLV_SHIFT)
#define _PAGE_KERN	(PLV_KERN << _PAGE_PLV_SHIFT)
#define _PFN_MASK (~((_ULCAST_(1) << (PFN_PTE_SHIFT)) - 1) & \
		  ((_ULCAST_(1) << (_PAGE_PFN_END_SHIFT)) - 1))
#ifndef _CACHE_SUC
#define _CACHE_SUC			(0<<_CACHE_SHIFT)  
#endif
#ifndef _CACHE_CC
#define _CACHE_CC			(1<<_CACHE_SHIFT)  
#endif
#ifndef _CACHE_WUC
#define _CACHE_WUC			(2<<_CACHE_SHIFT)  
#endif
#define __READABLE	(_PAGE_VALID)
#define __WRITEABLE	(_PAGE_DIRTY | _PAGE_WRITE)
#define _PAGE_CHG_MASK	(_PAGE_MODIFIED | _PAGE_SPECIAL | _PFN_MASK | _CACHE_MASK | _PAGE_PLV)
#define _HPAGE_CHG_MASK	(_PAGE_MODIFIED | _PAGE_SPECIAL | _PFN_MASK | _CACHE_MASK | _PAGE_PLV | _PAGE_HUGE)
#define PAGE_NONE	__pgprot(_PAGE_PROTNONE | _PAGE_NO_READ | \
				 _PAGE_USER | _CACHE_CC)
#define PAGE_SHARED	__pgprot(_PAGE_PRESENT | _PAGE_WRITE | \
				 _PAGE_USER | _CACHE_CC)
#define PAGE_READONLY	__pgprot(_PAGE_PRESENT | _PAGE_USER | _CACHE_CC)
#define PAGE_KERNEL	__pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN | _CACHE_CC)
#define PAGE_KERNEL_SUC __pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN |  _CACHE_SUC)
#define PAGE_KERNEL_WUC __pgprot(_PAGE_PRESENT | __READABLE | __WRITEABLE | \
				 _PAGE_GLOBAL | _PAGE_KERN |  _CACHE_WUC)
#ifndef __ASSEMBLY__
#define _PAGE_IOREMAP		pgprot_val(PAGE_KERNEL_SUC)
#define pgprot_noncached pgprot_noncached
static inline pgprot_t pgprot_noncached(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);
	prot = (prot & ~_CACHE_MASK) | _CACHE_SUC;
	return __pgprot(prot);
}
extern bool wc_enabled;
#define pgprot_writecombine pgprot_writecombine
static inline pgprot_t pgprot_writecombine(pgprot_t _prot)
{
	unsigned long prot = pgprot_val(_prot);
	prot = (prot & ~_CACHE_MASK) | (wc_enabled ? _CACHE_WUC : _CACHE_SUC);
	return __pgprot(prot);
}
#endif  
#endif  
