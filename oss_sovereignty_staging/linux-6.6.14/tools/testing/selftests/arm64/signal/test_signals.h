 
 

#ifndef __TEST_SIGNALS_H__
#define __TEST_SIGNALS_H__

#include <signal.h>
#include <stdbool.h>
#include <ucontext.h>

 
#include <asm/ptrace.h>
#include <asm/hwcap.h>

#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define get_regval(regname, out)			\
{							\
	asm volatile("mrs %0, " __stringify(regname)	\
	: "=r" (out)					\
	:						\
	: "memory");					\
}

 
enum {
	FSSBS_BIT,
	FSVE_BIT,
	FSME_BIT,
	FSME_FA64_BIT,
	FSME2_BIT,
	FMAX_END
};

#define FEAT_SSBS		(1UL << FSSBS_BIT)
#define FEAT_SVE		(1UL << FSVE_BIT)
#define FEAT_SME		(1UL << FSME_BIT)
#define FEAT_SME_FA64		(1UL << FSME_FA64_BIT)
#define FEAT_SME2		(1UL << FSME2_BIT)

 
struct tdescr {
	 
	void			*token;
	 
	bool			sanity_disabled;
	 
	char			*name;
	char			*descr;
	unsigned long		feats_required;
	unsigned long		feats_incompatible;
	 
	unsigned long		feats_supported;
	bool			initialized;
	unsigned int		minsigstksz;
	 
	int			sig_trig;
	 
	int			sig_ok;
	 
	int			sig_unsupp;
	 
	unsigned int		timeout;
	bool			triggered;
	bool			pass;
	unsigned int		result;
	 
	int			sa_flags;
	ucontext_t		saved_uc;
	 
	size_t			live_sz;
	ucontext_t		*live_uc;
	volatile sig_atomic_t	live_uc_valid;
	 
	void			*priv;

	 
	int (*setup)(struct tdescr *td);
	 
	bool (*init)(struct tdescr *td);
	 
	void (*cleanup)(struct tdescr *td);
	 
	int (*trigger)(struct tdescr *td);
	 
	int (*run)(struct tdescr *td, siginfo_t *si, ucontext_t *uc);
	 
	void (*check_result)(struct tdescr *td);
};

extern struct tdescr tde;
#endif
