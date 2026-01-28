#ifndef _ASM_S390_NMI_H
#define _ASM_S390_NMI_H
#include <linux/bits.h>
#include <linux/types.h>
#define MCIC_SUBCLASS_MASK	(1ULL<<63 | 1ULL<<62 | 1ULL<<61 | \
				1ULL<<59 | 1ULL<<58 | 1ULL<<56 | \
				1ULL<<55 | 1ULL<<54 | 1ULL<<53 | \
				1ULL<<52 | 1ULL<<47 | 1ULL<<46 | \
				1ULL<<45 | 1ULL<<44)
#define MCCK_CODE_SYSTEM_DAMAGE		BIT(63)
#define MCCK_CODE_EXT_DAMAGE		BIT(63 - 5)
#define MCCK_CODE_CP			BIT(63 - 9)
#define MCCK_CODE_STG_ERROR		BIT(63 - 16)
#define MCCK_CODE_STG_KEY_ERROR		BIT(63 - 18)
#define MCCK_CODE_STG_DEGRAD		BIT(63 - 19)
#define MCCK_CODE_PSW_MWP_VALID		BIT(63 - 20)
#define MCCK_CODE_PSW_IA_VALID		BIT(63 - 23)
#define MCCK_CODE_STG_FAIL_ADDR		BIT(63 - 24)
#define MCCK_CODE_CR_VALID		BIT(63 - 29)
#define MCCK_CODE_GS_VALID		BIT(63 - 36)
#define MCCK_CODE_FC_VALID		BIT(63 - 43)
#define MCCK_CODE_CPU_TIMER_VALID	BIT(63 - 46)
#ifndef __ASSEMBLY__
union mci {
	unsigned long val;
	struct {
		u64 sd :  1;  
		u64 pd :  1;  
		u64 sr :  1;  
		u64    :  1;  
		u64 cd :  1;  
		u64 ed :  1;  
		u64    :  1;  
		u64 dg :  1;  
		u64 w  :  1;  
		u64 cp :  1;  
		u64 sp :  1;  
		u64 ck :  1;  
		u64    :  2;  
		u64 b  :  1;  
		u64    :  1;  
		u64 se :  1;  
		u64 sc :  1;  
		u64 ke :  1;  
		u64 ds :  1;  
		u64 wp :  1;  
		u64 ms :  1;  
		u64 pm :  1;  
		u64 ia :  1;  
		u64 fa :  1;  
		u64 vr :  1;  
		u64 ec :  1;  
		u64 fp :  1;  
		u64 gr :  1;  
		u64 cr :  1;  
		u64    :  1;  
		u64 st :  1;  
		u64 ie :  1;  
		u64 ar :  1;  
		u64 da :  1;  
		u64    :  1;  
		u64 gs :  1;  
		u64    :  5;  
		u64 pr :  1;  
		u64 fc :  1;  
		u64 ap :  1;  
		u64    :  1;  
		u64 ct :  1;  
		u64 cc :  1;  
		u64    : 16;  
	};
};
#define MCESA_ORIGIN_MASK	(~0x3ffUL)
#define MCESA_LC_MASK		(0xfUL)
#define MCESA_MIN_SIZE		(1024)
#define MCESA_MAX_SIZE		(2048)
struct mcesa {
	u8 vector_save_area[1024];
	u8 guarded_storage_save_area[32];
};
struct pt_regs;
void nmi_alloc_mcesa_early(u64 *mcesad);
int nmi_alloc_mcesa(u64 *mcesad);
void nmi_free_mcesa(u64 *mcesad);
void s390_handle_mcck(void);
void s390_do_machine_check(struct pt_regs *regs);
#endif  
#endif  
