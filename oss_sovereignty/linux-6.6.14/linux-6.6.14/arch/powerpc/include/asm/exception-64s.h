#ifndef _ASM_POWERPC_EXCEPTION_H
#define _ASM_POWERPC_EXCEPTION_H
#include <asm/feature-fixups.h>
#define EX_SIZE		10
#define EX_R9		0
#define EX_R10		8
#define EX_R11		16
#define EX_R12		24
#define EX_R13		32
#define EX_DAR		40
#define EX_DSISR	48
#define EX_CCR		52
#define EX_CFAR		56
#define EX_PPR		64
#define EX_CTR		72
#define MAX_MCE_DEPTH	4
#ifdef __ASSEMBLY__
#define STF_ENTRY_BARRIER_SLOT						\
	STF_ENTRY_BARRIER_FIXUP_SECTION;				\
	nop;								\
	nop;								\
	nop
#define STF_EXIT_BARRIER_SLOT						\
	STF_EXIT_BARRIER_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop;								\
	nop;								\
	nop;								\
	nop
#define ENTRY_FLUSH_SLOT						\
	ENTRY_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop;
#define SCV_ENTRY_FLUSH_SLOT						\
	SCV_ENTRY_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop;
#define INTERRUPT_TO_KERNEL						\
	STF_ENTRY_BARRIER_SLOT;						\
	ENTRY_FLUSH_SLOT
#define SCV_INTERRUPT_TO_KERNEL						\
	STF_ENTRY_BARRIER_SLOT;						\
	SCV_ENTRY_FLUSH_SLOT
#define RFI_FLUSH_SLOT							\
	RFI_FLUSH_FIXUP_SECTION;					\
	nop;								\
	nop;								\
	nop
#define RFI_TO_KERNEL							\
	rfid
#define RFI_TO_USER							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback
#define RFI_TO_USER_OR_KERNEL						\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback
#define RFI_TO_GUEST							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	rfid;								\
	b	rfi_flush_fallback
#define HRFI_TO_KERNEL							\
	hrfid
#define HRFI_TO_USER							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback
#define HRFI_TO_USER_OR_KERNEL						\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback
#define HRFI_TO_GUEST							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback
#define HRFI_TO_UNKNOWN							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	hrfid;								\
	b	hrfi_flush_fallback
#define RFSCV_TO_USER							\
	STF_EXIT_BARRIER_SLOT;						\
	RFI_FLUSH_SLOT;							\
	RFSCV;								\
	b	rfscv_flush_fallback
#else  
void do_uaccess_flush(void);
#endif  
#endif	 
