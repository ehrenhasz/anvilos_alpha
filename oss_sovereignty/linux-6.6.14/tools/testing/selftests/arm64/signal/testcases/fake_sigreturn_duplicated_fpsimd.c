
 

#include <signal.h>
#include <ucontext.h>

#include "test_signals_utils.h"
#include "testcases.h"

struct fake_sigframe sf;

static int fake_sigreturn_duplicated_fpsimd_run(struct tdescr *td,
						siginfo_t *si, ucontext_t *uc)
{
	struct _aarch64_ctx *shead = GET_SF_RESV_HEAD(sf), *head;

	 
	if (!get_current_context(td, &sf.uc, sizeof(sf.uc)))
		return 1;

	head = get_starting_head(shead, sizeof(struct fpsimd_context) + HDR_SZ,
				 GET_SF_RESV_SIZE(sf), NULL);
	if (!head)
		return 0;

	 
	head->magic = FPSIMD_MAGIC;
	head->size = sizeof(struct fpsimd_context);
	 
	write_terminator_record(GET_RESV_NEXT_HEAD(head));

	ASSERT_BAD_CONTEXT(&sf.uc);
	fake_sigreturn(&sf, sizeof(sf), 0);

	return 1;
}

struct tdescr tde = {
		.name = "FAKE_SIGRETURN_DUPLICATED_FPSIMD",
		.descr = "Triggers a sigreturn including two fpsimd_context",
		.sig_ok = SIGSEGV,
		.timeout = 3,
		.run = fake_sigreturn_duplicated_fpsimd_run,
};
