#ifndef _ASM_POWERPC_NOHASH_32_PTE_40x_H
#define _ASM_POWERPC_NOHASH_32_PTE_40x_H
#ifdef __KERNEL__
#define	_PAGE_GUARDED	0x001	 
#define _PAGE_PRESENT	0x002	 
#define	_PAGE_NO_CACHE	0x004	 
#define	_PAGE_WRITETHRU	0x008	 
#define	_PAGE_USER	0x010	 
#define	_PAGE_SPECIAL	0x020	 
#define	_PAGE_DIRTY	0x080	 
#define _PAGE_RW	0x100	 
#define _PAGE_EXEC	0x200	 
#define _PAGE_ACCESSED	0x400	 
#define _PAGE_PSIZE		0
#define _PAGE_COHERENT	0
#define _PAGE_KERNEL_RO		0
#define _PAGE_KERNEL_ROX	_PAGE_EXEC
#define _PAGE_KERNEL_RW		(_PAGE_DIRTY | _PAGE_RW)
#define _PAGE_KERNEL_RWX	(_PAGE_DIRTY | _PAGE_RW | _PAGE_EXEC)
#define _PMD_PRESENT	0x400	 
#define _PMD_PRESENT_MASK	_PMD_PRESENT
#define _PMD_BAD	0x802
#define _PMD_SIZE_4M	0x0c0
#define _PMD_SIZE_16M	0x0e0
#define _PMD_USER	0
#define _PTE_NONE_MASK	0
#define _PAGE_BASE_NC	(_PAGE_PRESENT | _PAGE_ACCESSED)
#define _PAGE_BASE	(_PAGE_BASE_NC)
#define PAGE_NONE	__pgprot(_PAGE_BASE)
#define PAGE_SHARED	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW)
#define PAGE_SHARED_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_RW | _PAGE_EXEC)
#define PAGE_COPY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_COPY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#define PAGE_READONLY	__pgprot(_PAGE_BASE | _PAGE_USER)
#define PAGE_READONLY_X	__pgprot(_PAGE_BASE | _PAGE_USER | _PAGE_EXEC)
#endif  
#endif  
