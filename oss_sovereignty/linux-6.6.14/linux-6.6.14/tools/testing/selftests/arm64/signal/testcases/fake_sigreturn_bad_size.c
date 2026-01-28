#include <signal.h>
#include <ucontext.h>
#include "test_signals_utils.h"
#include "testcases.h"
struct fake_sigframe sf;
#define MIN_SZ_ALIGN	16
static int fake_sigreturn_bad_size_run(struct tdescr *td,
				       siginfo_t *si, ucontext_t *uc)
{
	size_t resv_sz, need_sz, offset;
	struct _aarch64_ctx *shead = GET_SF_RESV_HEAD(sf), *head;
	if (!get_current_context(td, &sf.uc, sizeof(sf.uc)))
		return 1;
	resv_sz = GET_SF_RESV_SIZE(sf);
	need_sz = sizeof(struct esr_context) + HDR_SZ;
	head = get_starting_head(shead, need_sz, resv_sz, &offset);
	if (!head)
		return 0;
	head->magic = ESR_MAGIC;
	head->size = sizeof(struct esr_context);
	write_terminator_record(GET_RESV_NEXT_HEAD(head));
	ASSERT_GOOD_CONTEXT(&sf.uc);
	head->size = (resv_sz - offset - need_sz + MIN_SZ_ALIGN) & ~0xfUL;
	head->size += MIN_SZ_ALIGN;
	write_terminator_record(GET_RESV_NEXT_HEAD(head));
	ASSERT_BAD_CONTEXT(&sf.uc);
	fake_sigreturn(&sf, sizeof(sf), 0);
	return 1;
}
struct tdescr tde = {
		.name = "FAKE_SIGRETURN_BAD_SIZE",
		.descr = "Triggers a sigreturn with a overrun __reserved area",
		.sig_ok = SIGSEGV,
		.timeout = 3,
		.run = fake_sigreturn_bad_size_run,
};
