#ifndef _ASM_S390_PAI_H
#define _ASM_S390_PAI_H
#include <linux/jump_label.h>
#include <asm/lowcore.h>
#include <asm/ptrace.h>
struct qpaci_info_block {
	u64 header;
	struct {
		u64 : 8;
		u64 num_cc : 8;	 
		u64 : 9;
		u64 num_nnpa : 7;	 
		u64 : 32;
	};
};
static inline int qpaci(struct qpaci_info_block *info)
{
	size_t size = sizeof(*info) / sizeof(u64) - 1;
	int cc;
	asm volatile(
		"	lgr	0,%[size]\n"
		"	.insn	s,0xb28f0000,%[info]\n"
		"	lgr	%[size],0\n"
		"	ipm	%[cc]\n"
		"	srl	%[cc],28\n"
		: [cc] "=d" (cc), [info] "=Q" (*info), [size] "+&d" (size)
		:
		: "0", "cc", "memory");
	return cc ? (size + 1) * sizeof(u64) : 0;
}
#define PAI_CRYPTO_BASE			0x1000	 
#define PAI_CRYPTO_MAXCTR		256	 
#define PAI_CRYPTO_KERNEL_OFFSET	2048
#define PAI_NNPA_BASE			0x1800	 
#define PAI_NNPA_MAXCTR			128	 
DECLARE_STATIC_KEY_FALSE(pai_key);
static __always_inline void pai_kernel_enter(struct pt_regs *regs)
{
	if (!IS_ENABLED(CONFIG_PERF_EVENTS))
		return;
	if (!static_branch_unlikely(&pai_key))
		return;
	if (!S390_lowcore.ccd)
		return;
	if (!user_mode(regs))
		return;
	WRITE_ONCE(S390_lowcore.ccd, S390_lowcore.ccd | PAI_CRYPTO_KERNEL_OFFSET);
}
static __always_inline void pai_kernel_exit(struct pt_regs *regs)
{
	if (!IS_ENABLED(CONFIG_PERF_EVENTS))
		return;
	if (!static_branch_unlikely(&pai_key))
		return;
	if (!S390_lowcore.ccd)
		return;
	if (!user_mode(regs))
		return;
	WRITE_ONCE(S390_lowcore.ccd, S390_lowcore.ccd & ~PAI_CRYPTO_KERNEL_OFFSET);
}
enum paievt_mode {
	PAI_MODE_NONE,
	PAI_MODE_SAMPLING,
	PAI_MODE_COUNTING,
};
#endif
