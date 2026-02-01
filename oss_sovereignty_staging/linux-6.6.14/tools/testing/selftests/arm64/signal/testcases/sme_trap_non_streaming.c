
 

#include <signal.h>
#include <ucontext.h>
#include <sys/prctl.h>

#include "test_signals_utils.h"
#include "testcases.h"

int sme_trap_non_streaming_trigger(struct tdescr *td)
{
	 
	asm volatile(".inst 0xd503437f;   \
		      cnt v0.16b, v0.16b; \
                      .inst 0xd503447f   ");

	return 0;
}

int sme_trap_non_streaming_run(struct tdescr *td, siginfo_t *si, ucontext_t *uc)
{
	return 1;
}

struct tdescr tde = {
	.name = "SME SM trap unsupported instruction",
	.descr = "Check that we get a SIGILL if we use an unsupported instruction in streaming mode",
	.feats_required = FEAT_SME,
	.feats_incompatible = FEAT_SME_FA64,
	.timeout = 3,
	.sanity_disabled = true,
	.trigger = sme_trap_non_streaming_trigger,
	.run = sme_trap_non_streaming_run,
	.sig_ok = SIGILL,
};
