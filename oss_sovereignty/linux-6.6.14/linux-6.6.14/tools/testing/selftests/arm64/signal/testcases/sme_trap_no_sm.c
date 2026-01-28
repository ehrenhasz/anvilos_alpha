#include <signal.h>
#include <ucontext.h>
#include <sys/prctl.h>
#include "test_signals_utils.h"
#include "testcases.h"
int sme_trap_no_sm_trigger(struct tdescr *td)
{
	asm volatile(".inst 0xd503457f ; .inst 0xc0900000");
	return 0;
}
int sme_trap_no_sm_run(struct tdescr *td, siginfo_t *si, ucontext_t *uc)
{
	return 1;
}
struct tdescr tde = {
	.name = "SME trap without SM",
	.descr = "Check that we get a SIGILL if we use streaming mode without enabling it",
	.timeout = 3,
	.feats_required = FEAT_SME,    
	.sanity_disabled = true,
	.trigger = sme_trap_no_sm_trigger,
	.run = sme_trap_no_sm_run,
	.sig_ok = SIGILL,
};
