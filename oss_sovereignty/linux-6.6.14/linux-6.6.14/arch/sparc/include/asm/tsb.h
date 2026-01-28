#ifndef _SPARC64_TSB_H
#define _SPARC64_TSB_H
#define TSB_TAG_LOCK_BIT	47
#define TSB_TAG_LOCK_HIGH	(1 << (TSB_TAG_LOCK_BIT - 32))
#define TSB_TAG_INVALID_BIT	46
#define TSB_TAG_INVALID_HIGH	(1 << (TSB_TAG_INVALID_BIT - 32))
#ifndef __ASSEMBLY__
struct tsb_ldquad_phys_patch_entry {
	unsigned int	addr;
	unsigned int	sun4u_insn;
	unsigned int	sun4v_insn;
};
extern struct tsb_ldquad_phys_patch_entry __tsb_ldquad_phys_patch,
	__tsb_ldquad_phys_patch_end;
struct tsb_phys_patch_entry {
	unsigned int	addr;
	unsigned int	insn;
};
extern struct tsb_phys_patch_entry __tsb_phys_patch, __tsb_phys_patch_end;
#endif
#define TSB_LOAD_QUAD(TSB, REG)	\
661:	ldda		[TSB] ASI_NUCLEUS_QUAD_LDD, REG; \
	.section	.tsb_ldquad_phys_patch, "ax"; \
	.word		661b; \
	ldda		[TSB] ASI_QUAD_LDD_PHYS, REG; \
	ldda		[TSB] ASI_QUAD_LDD_PHYS_4V, REG; \
	.previous
#define TSB_LOAD_TAG_HIGH(TSB, REG) \
661:	lduwa		[TSB] ASI_N, REG; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	lduwa		[TSB] ASI_PHYS_USE_EC, REG; \
	.previous
#define TSB_LOAD_TAG(TSB, REG) \
661:	ldxa		[TSB] ASI_N, REG; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	ldxa		[TSB] ASI_PHYS_USE_EC, REG; \
	.previous
#define TSB_CAS_TAG_HIGH(TSB, REG1, REG2) \
661:	casa		[TSB] ASI_N, REG1, REG2; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	casa		[TSB] ASI_PHYS_USE_EC, REG1, REG2; \
	.previous
#define TSB_CAS_TAG(TSB, REG1, REG2) \
661:	casxa		[TSB] ASI_N, REG1, REG2; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	casxa		[TSB] ASI_PHYS_USE_EC, REG1, REG2; \
	.previous
#define TSB_STORE(ADDR, VAL) \
661:	stxa		VAL, [ADDR] ASI_N; \
	.section	.tsb_phys_patch, "ax"; \
	.word		661b; \
	stxa		VAL, [ADDR] ASI_PHYS_USE_EC; \
	.previous
#define TSB_LOCK_TAG(TSB, REG1, REG2)	\
99:	TSB_LOAD_TAG_HIGH(TSB, REG1);	\
	sethi	%hi(TSB_TAG_LOCK_HIGH), REG2;\
	andcc	REG1, REG2, %g0;	\
	bne,pn	%icc, 99b;		\
	 nop;				\
	TSB_CAS_TAG_HIGH(TSB, REG1, REG2);	\
	cmp	REG1, REG2;		\
	bne,pn	%icc, 99b;		\
	 nop;				\
#define TSB_WRITE(TSB, TTE, TAG) \
	add	TSB, 0x8, TSB;   \
	TSB_STORE(TSB, TTE);     \
	sub	TSB, 0x8, TSB;   \
	TSB_STORE(TSB, TAG);
#define KERN_PGTABLE_WALK(VADDR, REG1, REG2, FAIL_LABEL)	\
	sethi		%hi(swapper_pg_dir), REG1; \
	or		REG1, %lo(swapper_pg_dir), REG1; \
	sllx		VADDR, 64 - (PGDIR_SHIFT + PGDIR_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldx		[REG1 + REG2], REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - (PUD_SHIFT + PUD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	sethi		%uhi(_PAGE_PUD_HUGE), REG2; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		REG2, 32, REG2; \
	andcc		REG1, REG2, %g0; \
	sethi		%hi(0xf8000000), REG2; \
	bne,pt		%xcc, 697f; \
	 sllx		REG2, 1, REG2; \
	sllx		VADDR, 64 - (PMD_SHIFT + PMD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	sethi		%uhi(_PAGE_PMD_HUGE), REG2; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		REG2, 32, REG2; \
	andcc		REG1, REG2, %g0; \
	be,pn		%xcc, 698f; \
	 sethi		%hi(0x400000), REG2; \
697:	brgez,pn	REG1, FAIL_LABEL; \
	 andn		REG1, REG2, REG1; \
	and		VADDR, REG2, REG2; \
	ba,pt		%xcc, 699f; \
	 or		REG1, REG2, REG1; \
698:	sllx		VADDR, 64 - PMD_SHIFT, REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	brgez,pn	REG1, FAIL_LABEL; \
	 nop; \
699:
#if defined(CONFIG_HUGETLB_PAGE) || defined(CONFIG_TRANSPARENT_HUGEPAGE)
#define USER_PGTABLE_CHECK_PUD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, PTE_LABEL) \
700:	ba 700f;					\
	 nop;						\
	.section	.pud_huge_patch, "ax";		\
	.word		700b;				\
	nop;						\
	.previous;					\
	brz,pn		REG1, FAIL_LABEL;		\
	 sethi		%uhi(_PAGE_PUD_HUGE), REG2;	\
	sllx		REG2, 32, REG2;			\
	andcc		REG1, REG2, %g0;		\
	be,pt		%xcc, 700f;			\
	 sethi		%hi(0xffe00000), REG2;		\
	sllx		REG2, 1, REG2;			\
	brgez,pn	REG1, FAIL_LABEL;		\
	 andn		REG1, REG2, REG1;		\
	and		VADDR, REG2, REG2;		\
	brlz,pt		REG1, PTE_LABEL;		\
	 or		REG1, REG2, REG1;		\
700:
#else
#define USER_PGTABLE_CHECK_PUD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, PTE_LABEL) \
	brz,pn		REG1, FAIL_LABEL; \
	 nop;
#endif
#if defined(CONFIG_HUGETLB_PAGE) || defined(CONFIG_TRANSPARENT_HUGEPAGE)
#define USER_PGTABLE_CHECK_PMD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, PTE_LABEL) \
	brz,pn		REG1, FAIL_LABEL;		\
	 sethi		%uhi(_PAGE_PMD_HUGE), REG2;	\
	sllx		REG2, 32, REG2;			\
	andcc		REG1, REG2, %g0;		\
	be,pt		%xcc, 700f;			\
	 sethi		%hi(4 * 1024 * 1024), REG2;	\
	brgez,pn	REG1, FAIL_LABEL;		\
	 andn		REG1, REG2, REG1;		\
	and		VADDR, REG2, REG2;		\
	brlz,pt		REG1, PTE_LABEL;		\
	 or		REG1, REG2, REG1;		\
700:
#else
#define USER_PGTABLE_CHECK_PMD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, PTE_LABEL) \
	brz,pn		REG1, FAIL_LABEL; \
	 nop;
#endif
#define USER_PGTABLE_WALK_TL1(VADDR, PHYS_PGD, REG1, REG2, FAIL_LABEL)	\
	sllx		VADDR, 64 - (PGDIR_SHIFT + PGDIR_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[PHYS_PGD + REG2] ASI_PHYS_USE_EC, REG1; \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - (PUD_SHIFT + PUD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	USER_PGTABLE_CHECK_PUD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, 800f) \
	brz,pn		REG1, FAIL_LABEL; \
	 sllx		VADDR, 64 - (PMD_SHIFT + PMD_BITS), REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	ldxa		[REG1 + REG2] ASI_PHYS_USE_EC, REG1; \
	USER_PGTABLE_CHECK_PMD_HUGE(VADDR, REG1, REG2, FAIL_LABEL, 800f) \
	sllx		VADDR, 64 - PMD_SHIFT, REG2; \
	srlx		REG2, 64 - PAGE_SHIFT, REG2; \
	andn		REG2, 0x7, REG2; \
	add		REG1, REG2, REG1; \
	ldxa		[REG1] ASI_PHYS_USE_EC, REG1; \
	brgez,pn	REG1, FAIL_LABEL; \
	 nop; \
800:
#define OBP_TRANS_LOOKUP(VADDR, REG1, REG2, REG3, FAIL_LABEL) \
	sethi		%hi(prom_trans), REG1; \
	or		REG1, %lo(prom_trans), REG1; \
97:	ldx		[REG1 + 0x00], REG2; \
	brz,pn		REG2, FAIL_LABEL; \
	 nop; \
	ldx		[REG1 + 0x08], REG3; \
	add		REG2, REG3, REG3; \
	cmp		REG2, VADDR; \
	bgu,pt		%xcc, 98f; \
	 cmp		VADDR, REG3; \
	bgeu,pt		%xcc, 98f; \
	 ldx		[REG1 + 0x10], REG3; \
	sub		VADDR, REG2, REG2; \
	ba,pt		%xcc, 99f; \
	 add		REG3, REG2, REG1; \
98:	ba,pt		%xcc, 97b; \
	 add		REG1, (3 * 8), REG1; \
99:
#define KERNEL_TSB_SIZE_BYTES	(32 * 1024)
#define KERNEL_TSB_NENTRIES	\
	(KERNEL_TSB_SIZE_BYTES / 16)
#define KERNEL_TSB4M_NENTRIES	4096
#define KERN_TSB_LOOKUP_TL1(VADDR, TAG, REG1, REG2, REG3, REG4, OK_LABEL) \
661:	sethi		%uhi(swapper_tsb), REG1; \
	sethi		%hi(swapper_tsb), REG2; \
	or		REG1, %ulo(swapper_tsb), REG1; \
	or		REG2, %lo(swapper_tsb), REG2; \
	.section	.swapper_tsb_phys_patch, "ax"; \
	.word		661b; \
	.previous; \
	sllx		REG1, 32, REG1; \
	or		REG1, REG2, REG1; \
	srlx		VADDR, PAGE_SHIFT, REG2; \
	and		REG2, (KERNEL_TSB_NENTRIES - 1), REG2; \
	sllx		REG2, 4, REG2; \
	add		REG1, REG2, REG2; \
	TSB_LOAD_QUAD(REG2, REG3); \
	cmp		REG3, TAG; \
	be,a,pt		%xcc, OK_LABEL; \
	 mov		REG4, REG1;
#ifndef CONFIG_DEBUG_PAGEALLOC
#define KERN_TSB4M_LOOKUP_TL1(TAG, REG1, REG2, REG3, REG4, OK_LABEL) \
661:	sethi		%uhi(swapper_4m_tsb), REG1; \
	sethi		%hi(swapper_4m_tsb), REG2; \
	or		REG1, %ulo(swapper_4m_tsb), REG1; \
	or		REG2, %lo(swapper_4m_tsb), REG2; \
	.section	.swapper_4m_tsb_phys_patch, "ax"; \
	.word		661b; \
	.previous; \
	sllx		REG1, 32, REG1; \
	or		REG1, REG2, REG1; \
	and		TAG, (KERNEL_TSB4M_NENTRIES - 1), REG2; \
	sllx		REG2, 4, REG2; \
	add		REG1, REG2, REG2; \
	TSB_LOAD_QUAD(REG2, REG3); \
	cmp		REG3, TAG; \
	be,a,pt		%xcc, OK_LABEL; \
	 mov		REG4, REG1;
#endif
#endif  
