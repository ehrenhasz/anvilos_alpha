#ifndef _ASM_POWERPC_EXCEPTION_64E_H
#define _ASM_POWERPC_EXCEPTION_64E_H
#define PACA_EXGDBELL PACA_EXGEN
#define EX_R1		(0 * 8)
#define EX_CR		(1 * 8)
#define EX_R10		(2 * 8)
#define EX_R11		(3 * 8)
#define EX_R14		(4 * 8)
#define EX_R15		(5 * 8)
#define EX_TLB_R10	( 0 * 8)
#define EX_TLB_R11	( 1 * 8)
#define EX_TLB_R14	( 2 * 8)
#define EX_TLB_R15	( 3 * 8)
#define EX_TLB_R16	( 4 * 8)
#define EX_TLB_CR	( 5 * 8)
#define EX_TLB_R12	( 6 * 8)
#define EX_TLB_R13	( 7 * 8)
#define EX_TLB_DEAR	( 8 * 8)  
#define EX_TLB_ESR	( 9 * 8)  
#define EX_TLB_SRR0	(10 * 8)
#define EX_TLB_SRR1	(11 * 8)
#define EX_TLB_R7	(12 * 8)
#define EX_TLB_SIZE	(13 * 8)
#define	START_EXCEPTION(label)						\
	.globl exc_##label##_book3e;					\
exc_##label##_book3e:
#define TLB_MISS_PROLOG							    \
	mtspr	SPRN_SPRG_TLB_SCRATCH,r12;				    \
	mfspr	r12,SPRN_SPRG_TLB_EXFRAME;				    \
	std	r10,EX_TLB_R10(r12);					    \
	mfcr	r10;							    \
	std	r11,EX_TLB_R11(r12);					    \
	mfspr	r11,SPRN_SPRG_TLB_SCRATCH;				    \
	std	r13,EX_TLB_R13(r12);					    \
	mfspr	r13,SPRN_SPRG_PACA;					    \
	std	r14,EX_TLB_R14(r12);					    \
	addi	r14,r12,EX_TLB_SIZE;					    \
	std	r15,EX_TLB_R15(r12);					    \
	mfspr	r15,SPRN_SRR1;						    \
	std	r16,EX_TLB_R16(r12);					    \
	mfspr	r16,SPRN_SRR0;						    \
	std	r10,EX_TLB_CR(r12);					    \
	std	r11,EX_TLB_R12(r12);					    \
	mtspr	SPRN_SPRG_TLB_EXFRAME,r14;				    \
	std	r15,EX_TLB_SRR1(r12);					    \
	std	r16,EX_TLB_SRR0(r12);
#define TLB_MISS_RESTORE(freg)						    \
	ld	r14,EX_TLB_CR(r12);					    \
	ld	r10,EX_TLB_R10(r12);					    \
	ld	r15,EX_TLB_SRR0(r12);					    \
	ld	r16,EX_TLB_SRR1(r12);					    \
	mtspr	SPRN_SPRG_TLB_EXFRAME,freg;				    \
	ld	r11,EX_TLB_R11(r12);					    \
	mtcr	r14;							    \
	ld	r13,EX_TLB_R13(r12);					    \
	ld	r14,EX_TLB_R14(r12);					    \
	mtspr	SPRN_SRR0,r15;						    \
	ld	r15,EX_TLB_R15(r12);					    \
	mtspr	SPRN_SRR1,r16;						    \
	ld	r16,EX_TLB_R16(r12);					    \
	ld	r12,EX_TLB_R12(r12);					    \
#define TLB_MISS_EPILOG_SUCCESS						    \
	TLB_MISS_RESTORE(r12)
#define TLB_MISS_EPILOG_ERROR						    \
	addi	r12,r13,PACA_EXTLB;					    \
	TLB_MISS_RESTORE(r12)
#define TLB_MISS_EPILOG_ERROR_SPECIAL					    \
	addi	r11,r13,PACA_EXTLB;					    \
	TLB_MISS_RESTORE(r11)
#ifndef __ASSEMBLY__
extern unsigned int interrupt_base_book3e;
#endif
#define SET_IVOR(vector_number, vector_offset)	\
	LOAD_REG_ADDR(r3,interrupt_base_book3e);\
	ori	r3,r3,vector_offset@l;		\
	mtspr	SPRN_IVOR##vector_number,r3;
#define RFI_TO_KERNEL							\
	rfi
#define RFI_TO_USER							\
	rfi
#endif  
