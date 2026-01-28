#ifndef __TEST_SIGNALS_UTILS_H__
#define __TEST_SIGNALS_UTILS_H__
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <linux/compiler.h>
#include "test_signals.h"
int test_init(struct tdescr *td);
int test_setup(struct tdescr *td);
void test_cleanup(struct tdescr *td);
int test_run(struct tdescr *td);
void test_result(struct tdescr *td);
static inline bool feats_ok(struct tdescr *td)
{
	if (td->feats_incompatible & td->feats_supported)
		return false;
	return (td->feats_required & td->feats_supported) == td->feats_required;
}
static __always_inline bool get_current_context(struct tdescr *td,
						ucontext_t *dest_uc,
						size_t dest_sz)
{
	static volatile bool seen_already;
	int i;
	char *uc = (char *)dest_uc;
	assert(td && dest_uc);
	seen_already = 0;
	td->live_uc_valid = 0;
	td->live_sz = dest_sz;
	for (i = 0; i < td->live_sz; i++) {
		uc[i] = 0;
		OPTIMIZER_HIDE_VAR(uc[0]);
	}
	td->live_uc = dest_uc;
	asm volatile ("brk #666"
		      : "+m" (*dest_uc)
		      :
		      : "memory");
	if (td->feats_supported & FEAT_SME)
		asm volatile("msr S0_3_C4_C6_3, xzr");
	if (seen_already) {
		fprintf(stdout,
			"Unexpected successful sigreturn detected: live_uc is stale !\n");
		return 0;
	}
	seen_already = 1;
	return td->live_uc_valid;
}
int fake_sigreturn(void *sigframe, size_t sz, int misalign_bytes);
#endif
