


#include <signal.h>
#include <ucontext.h>

#include "test_signals_utils.h"
#include "testcases.h"

struct fake_sigframe sf;

static int fake_sigreturn_bad_size_for_magic0_run(struct tdescr *td,
						  siginfo_t *si, ucontext_t *uc)
{
	struct _aarch64_ctx *shead = GET_SF_RESV_HEAD(sf), *head;

	
	if (!get_current_context(td, &sf.uc, sizeof(sf.uc)))
		return 1;

	
	head = get_starting_head(shead, HDR_SZ, GET_SF_RESV_SIZE(sf), NULL);
	if (!head)
		return 0;

	head->magic = 0;
	head->size = HDR_SZ;
	ASSERT_BAD_CONTEXT(&sf.uc);
	fake_sigreturn(&sf, sizeof(sf), 0);

	return 1;
}

struct tdescr tde = {
		.name = "FAKE_SIGRETURN_BAD_SIZE_FOR_TERMINATOR",
		.descr = "Trigger a sigreturn using non-zero size terminator",
		.sig_ok = SIGSEGV,
		.timeout = 3,
		.run = fake_sigreturn_bad_size_for_magic0_run,
};
