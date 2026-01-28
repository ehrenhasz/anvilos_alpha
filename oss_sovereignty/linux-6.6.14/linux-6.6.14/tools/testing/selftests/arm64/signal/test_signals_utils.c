#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/auxv.h>
#include <linux/auxvec.h>
#include <ucontext.h>
#include <asm/unistd.h>
#include <kselftest.h>
#include "test_signals.h"
#include "test_signals_utils.h"
#include "testcases/testcases.h"
extern struct tdescr *current;
static int sig_copyctx = SIGTRAP;
static char const *const feats_names[FMAX_END] = {
	" SSBS ",
	" SVE ",
	" SME ",
	" FA64 ",
	" SME2 ",
};
#define MAX_FEATS_SZ	128
static char feats_string[MAX_FEATS_SZ];
static inline char *feats_to_string(unsigned long feats)
{
	size_t flen = MAX_FEATS_SZ - 1;
	feats_string[0] = '\0';
	for (int i = 0; i < FMAX_END; i++) {
		if (feats & (1UL << i)) {
			size_t tlen = strlen(feats_names[i]);
			assert(flen > tlen);
			flen -= tlen;
			strncat(feats_string, feats_names[i], flen);
		}
	}
	return feats_string;
}
static void unblock_signal(int signum)
{
	sigset_t sset;
	sigemptyset(&sset);
	sigaddset(&sset, signum);
	sigprocmask(SIG_UNBLOCK, &sset, NULL);
}
static void default_result(struct tdescr *td, bool force_exit)
{
	if (td->result == KSFT_SKIP) {
		fprintf(stderr, "==>> completed. SKIP.\n");
	} else if (td->pass) {
		fprintf(stderr, "==>> completed. PASS(1)\n");
		td->result = KSFT_PASS;
	} else {
		fprintf(stdout, "==>> completed. FAIL(0)\n");
		td->result = KSFT_FAIL;
	}
	if (force_exit)
		exit(td->result);
}
static bool handle_signal_unsupported(struct tdescr *td,
				      siginfo_t *si, void *uc)
{
	if (feats_ok(td))
		return false;
	((ucontext_t *)uc)->uc_mcontext.pc += 4;
	if (!td->initialized) {
		fprintf(stderr,
			"Got SIG_UNSUPP @test_init. Ignore.\n");
	} else {
		fprintf(stderr,
			"-- RX SIG_UNSUPP on unsupported feat...OK\n");
		td->pass = 1;
		default_result(current, 1);
	}
	return true;
}
static bool handle_signal_trigger(struct tdescr *td,
				  siginfo_t *si, void *uc)
{
	td->triggered = 1;
	td->run(td, si, uc);
	return true;
}
static bool handle_signal_ok(struct tdescr *td,
			     siginfo_t *si, void *uc)
{
	assert(!td->sig_trig || td->triggered);
	fprintf(stderr,
		"SIG_OK -- SP:0x%llX  si_addr@:%p  si_code:%d  token@:%p  offset:%ld\n",
		((ucontext_t *)uc)->uc_mcontext.sp,
		si->si_addr, si->si_code, td->token, td->token - si->si_addr);
	if (!td->sanity_disabled && !td->token) {
		fprintf(stdout,
			"current->token ZEROED...test is probably broken!\n");
		abort();
	}
	if (td->sig_ok == SIGSEGV && si->si_code != SEGV_ACCERR) {
		fprintf(stdout,
			"si_code != SEGV_ACCERR...test is probably broken!\n");
		abort();
	}
	td->pass = 1;
	default_result(current, 1);
	return true;
}
static bool handle_signal_copyctx(struct tdescr *td,
				  siginfo_t *si, void *uc_in)
{
	ucontext_t *uc = uc_in;
	struct _aarch64_ctx *head;
	struct extra_context *extra, *copied_extra;
	size_t offset = 0;
	size_t to_copy;
	ASSERT_GOOD_CONTEXT(uc);
	uc->uc_mcontext.pc += 4;
	head = (struct _aarch64_ctx *)uc->uc_mcontext.__reserved;
	head = get_header(head, EXTRA_MAGIC, td->live_sz, &offset);
	if (head) {
		extra = (struct extra_context *)head;
		to_copy = __builtin_offsetof(ucontext_t,
					     uc_mcontext.__reserved);
		to_copy += offset + sizeof(struct extra_context) + 16;
		to_copy += extra->size;
		copied_extra = (struct extra_context *)&(td->live_uc->uc_mcontext.__reserved[offset]);
	} else {
		copied_extra = NULL;
		to_copy = sizeof(ucontext_t);
	}
	if (to_copy > td->live_sz) {
		fprintf(stderr,
			"Not enough space to grab context, %lu/%lu bytes\n",
			td->live_sz, to_copy);
		return false;
	}
	memcpy(td->live_uc, uc, to_copy);
	if (copied_extra)
		copied_extra->head.size = sizeof(*copied_extra) + 16;
	td->live_uc_valid = 1;
	fprintf(stderr,
		"%lu byte GOOD CONTEXT grabbed from sig_copyctx handler\n",
		to_copy);
	return true;
}
static void default_handler(int signum, siginfo_t *si, void *uc)
{
	if (current->sig_unsupp && signum == current->sig_unsupp &&
	    handle_signal_unsupported(current, si, uc)) {
		fprintf(stderr, "Handled SIG_UNSUPP\n");
	} else if (current->sig_trig && signum == current->sig_trig &&
		   handle_signal_trigger(current, si, uc)) {
		fprintf(stderr, "Handled SIG_TRIG\n");
	} else if (current->sig_ok && signum == current->sig_ok &&
		   handle_signal_ok(current, si, uc)) {
		fprintf(stderr, "Handled SIG_OK\n");
	} else if (signum == sig_copyctx && current->live_uc &&
		   handle_signal_copyctx(current, si, uc)) {
		fprintf(stderr, "Handled SIG_COPYCTX\n");
	} else {
		if (signum == SIGALRM && current->timeout) {
			fprintf(stderr, "-- Timeout !\n");
		} else {
			fprintf(stderr,
				"-- RX UNEXPECTED SIGNAL: %d code %d address %p\n",
				signum, si->si_code, si->si_addr);
		}
		default_result(current, 1);
	}
}
static int default_setup(struct tdescr *td)
{
	struct sigaction sa;
	sa.sa_sigaction = default_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sa.sa_flags |= td->sa_flags;
	sigemptyset(&sa.sa_mask);
	for (int sig = 1; sig < 32; sig++)
		sigaction(sig, &sa, NULL);
	for (int sig = SIGRTMIN; sig <= SIGRTMAX; sig++)
		sigaction(sig, &sa, NULL);
	if (td->sig_trig)
		unblock_signal(td->sig_trig);
	if (td->sig_ok)
		unblock_signal(td->sig_ok);
	if (td->sig_unsupp)
		unblock_signal(td->sig_unsupp);
	if (td->timeout) {
		unblock_signal(SIGALRM);
		alarm(td->timeout);
	}
	fprintf(stderr, "Registered handlers for all signals.\n");
	return 1;
}
static inline int default_trigger(struct tdescr *td)
{
	return !raise(td->sig_trig);
}
int test_init(struct tdescr *td)
{
	if (td->sig_trig == sig_copyctx) {
		fprintf(stdout,
			"Signal %d is RESERVED, cannot be used as a trigger. Aborting\n",
			sig_copyctx);
		return 0;
	}
	unblock_signal(sig_copyctx);
	td->minsigstksz = getauxval(AT_MINSIGSTKSZ);
	if (!td->minsigstksz)
		td->minsigstksz = MINSIGSTKSZ;
	fprintf(stderr, "Detected MINSTKSIGSZ:%d\n", td->minsigstksz);
	if (td->feats_required || td->feats_incompatible) {
		td->feats_supported = 0;
		if (getauxval(AT_HWCAP) & HWCAP_SSBS)
			td->feats_supported |= FEAT_SSBS;
		if (getauxval(AT_HWCAP) & HWCAP_SVE)
			td->feats_supported |= FEAT_SVE;
		if (getauxval(AT_HWCAP2) & HWCAP2_SME)
			td->feats_supported |= FEAT_SME;
		if (getauxval(AT_HWCAP2) & HWCAP2_SME_FA64)
			td->feats_supported |= FEAT_SME_FA64;
		if (getauxval(AT_HWCAP2) & HWCAP2_SME2)
			td->feats_supported |= FEAT_SME2;
		if (feats_ok(td)) {
			if (td->feats_required & td->feats_supported)
				fprintf(stderr,
					"Required Features: [%s] supported\n",
					feats_to_string(td->feats_required &
							td->feats_supported));
			if (!(td->feats_incompatible & td->feats_supported))
				fprintf(stderr,
					"Incompatible Features: [%s] absent\n",
					feats_to_string(td->feats_incompatible));
		} else {
			if ((td->feats_required & td->feats_supported) !=
			    td->feats_supported)
				fprintf(stderr,
					"Required Features: [%s] NOT supported\n",
					feats_to_string(td->feats_required &
							~td->feats_supported));
			if (td->feats_incompatible & td->feats_supported)
				fprintf(stderr,
					"Incompatible Features: [%s] supported\n",
					feats_to_string(td->feats_incompatible &
							~td->feats_supported));
			td->result = KSFT_SKIP;
			return 0;
		}
	}
	if (td->init && !td->init(td)) {
		fprintf(stderr, "FAILED Testcase initialization.\n");
		return 0;
	}
	td->initialized = 1;
	fprintf(stderr, "Testcase initialized.\n");
	return 1;
}
int test_setup(struct tdescr *td)
{
	assert(current);
	assert(td);
	assert(td->name);
	assert(td->run);
	td->result = KSFT_FAIL;
	if (td->setup)
		return td->setup(td);
	else
		return default_setup(td);
}
int test_run(struct tdescr *td)
{
	if (td->trigger)
		return td->trigger(td);
	else if (td->sig_trig)
		return default_trigger(td);
	else
		return td->run(td, NULL, NULL);
}
void test_result(struct tdescr *td)
{
	if (td->initialized && td->result != KSFT_SKIP && td->check_result)
		td->check_result(td);
	default_result(td, 0);
}
void test_cleanup(struct tdescr *td)
{
	if (td->cleanup)
		td->cleanup(td);
}
