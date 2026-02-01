


#define _GNU_SOURCE

#include <sys/auxv.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <sched.h>

#include "../../kselftest_harness.h"
#include "helper.h"

#define PAC_COLLISION_ATTEMPTS 10
 
#define PAC_MASK (~0xff80ffffffffffff)
#define ARBITRARY_VALUE (0x1234)
#define ASSERT_PAUTH_ENABLED() \
do { \
	unsigned long hwcaps = getauxval(AT_HWCAP); \
	  \
	if (!(hwcaps & HWCAP_PACA))					\
		SKIP(return, "PAUTH not enabled"); \
} while (0)
#define ASSERT_GENERIC_PAUTH_ENABLED() \
do { \
	unsigned long hwcaps = getauxval(AT_HWCAP); \
	  \
	if (!(hwcaps & HWCAP_PACG)) \
		SKIP(return, "Generic PAUTH not enabled");	\
} while (0)

void sign_specific(struct signatures *sign, size_t val)
{
	sign->keyia = keyia_sign(val);
	sign->keyib = keyib_sign(val);
	sign->keyda = keyda_sign(val);
	sign->keydb = keydb_sign(val);
}

void sign_all(struct signatures *sign, size_t val)
{
	sign->keyia = keyia_sign(val);
	sign->keyib = keyib_sign(val);
	sign->keyda = keyda_sign(val);
	sign->keydb = keydb_sign(val);
	sign->keyg  = keyg_sign(val);
}

int n_same(struct signatures *old, struct signatures *new, int nkeys)
{
	int res = 0;

	res += old->keyia == new->keyia;
	res += old->keyib == new->keyib;
	res += old->keyda == new->keyda;
	res += old->keydb == new->keydb;
	if (nkeys == NKEYS)
		res += old->keyg == new->keyg;

	return res;
}

int n_same_single_set(struct signatures *sign, int nkeys)
{
	size_t vals[nkeys];
	int same = 0;

	vals[0] = sign->keyia & PAC_MASK;
	vals[1] = sign->keyib & PAC_MASK;
	vals[2] = sign->keyda & PAC_MASK;
	vals[3] = sign->keydb & PAC_MASK;

	if (nkeys >= 4)
		vals[4] = sign->keyg & PAC_MASK;

	for (int i = 0; i < nkeys - 1; i++) {
		for (int j = i + 1; j < nkeys; j++) {
			if (vals[i] == vals[j])
				same += 1;
		}
	}
	return same;
}

int exec_sign_all(struct signatures *signed_vals, size_t val)
{
	int new_stdin[2];
	int new_stdout[2];
	int status;
	int i;
	ssize_t ret;
	pid_t pid;
	cpu_set_t mask;

	ret = pipe(new_stdin);
	if (ret == -1) {
		perror("pipe returned error");
		return -1;
	}

	ret = pipe(new_stdout);
	if (ret == -1) {
		perror("pipe returned error");
		return -1;
	}

	 
	sched_getaffinity(0, sizeof(mask), &mask);

	for (i = 0; i < sizeof(cpu_set_t); i++)
		if (CPU_ISSET(i, &mask))
			break;

	CPU_ZERO(&mask);
	CPU_SET(i, &mask);
	sched_setaffinity(0, sizeof(mask), &mask);

	pid = fork();
	
	if (pid == 0) {
		dup2(new_stdin[0], STDIN_FILENO);
		if (ret == -1) {
			perror("dup2 returned error");
			exit(1);
		}

		dup2(new_stdout[1], STDOUT_FILENO);
		if (ret == -1) {
			perror("dup2 returned error");
			exit(1);
		}

		close(new_stdin[0]);
		close(new_stdin[1]);
		close(new_stdout[0]);
		close(new_stdout[1]);

		ret = execl("exec_target", "exec_target", (char *)NULL);
		if (ret == -1) {
			perror("exec returned error");
			exit(1);
		}
	}

	close(new_stdin[0]);
	close(new_stdout[1]);

	ret = write(new_stdin[1], &val, sizeof(size_t));
	if (ret == -1) {
		perror("write returned error");
		return -1;
	}

	 
	waitpid(pid, &status, 0);
	if (WIFEXITED(status) == 0) {
		fprintf(stderr, "worker exited unexpectedly\n");
		return -1;
	}
	if (WEXITSTATUS(status) != 0) {
		fprintf(stderr, "worker exited with error\n");
		return -1;
	}

	ret = read(new_stdout[0], signed_vals, sizeof(struct signatures));
	if (ret == -1) {
		perror("read returned error");
		return -1;
	}

	return 0;
}

sigjmp_buf jmpbuf;
void pac_signal_handler(int signum, siginfo_t *si, void *uc)
{
	if (signum == SIGSEGV || signum == SIGILL)
		siglongjmp(jmpbuf, 1);
}

 
TEST(corrupt_pac)
{
	struct sigaction sa;

	ASSERT_PAUTH_ENABLED();
	if (sigsetjmp(jmpbuf, 1) == 0) {
		sa.sa_sigaction = pac_signal_handler;
		sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
		sigemptyset(&sa.sa_mask);

		sigaction(SIGSEGV, &sa, NULL);
		sigaction(SIGILL, &sa, NULL);

		pac_corruptor();
		ASSERT_TRUE(0) TH_LOG("SIGSEGV/SIGILL signal did not occur");
	}
}

 
TEST(pac_instructions_not_nop)
{
	size_t keyia = 0;
	size_t keyib = 0;
	size_t keyda = 0;
	size_t keydb = 0;

	ASSERT_PAUTH_ENABLED();

	for (int i = 0; i < PAC_COLLISION_ATTEMPTS; i++) {
		keyia |= keyia_sign(i) & PAC_MASK;
		keyib |= keyib_sign(i) & PAC_MASK;
		keyda |= keyda_sign(i) & PAC_MASK;
		keydb |= keydb_sign(i) & PAC_MASK;
	}

	ASSERT_NE(0, keyia) TH_LOG("keyia instructions did nothing");
	ASSERT_NE(0, keyib) TH_LOG("keyib instructions did nothing");
	ASSERT_NE(0, keyda) TH_LOG("keyda instructions did nothing");
	ASSERT_NE(0, keydb) TH_LOG("keydb instructions did nothing");
}

TEST(pac_instructions_not_nop_generic)
{
	size_t keyg = 0;

	ASSERT_GENERIC_PAUTH_ENABLED();

	for (int i = 0; i < PAC_COLLISION_ATTEMPTS; i++)
		keyg |= keyg_sign(i) & PAC_MASK;

	ASSERT_NE(0, keyg)  TH_LOG("keyg instructions did nothing");
}

TEST(single_thread_different_keys)
{
	int same = 10;
	int nkeys = NKEYS;
	int tmp;
	struct signatures signed_vals;
	unsigned long hwcaps = getauxval(AT_HWCAP);

	 
	ASSERT_PAUTH_ENABLED();
	if (!(hwcaps & HWCAP_PACG)) {
		TH_LOG("WARNING: Generic PAUTH not enabled. Skipping generic key checks");
		nkeys = NKEYS - 1;
	}

	 
	for (int i = 0; i < PAC_COLLISION_ATTEMPTS; i++) {
		if (nkeys == NKEYS)
			sign_all(&signed_vals, i);
		else
			sign_specific(&signed_vals, i);

		tmp = n_same_single_set(&signed_vals, nkeys);
		if (tmp < same)
			same = tmp;
	}

	ASSERT_EQ(0, same) TH_LOG("%d keys clashed every time", same);
}

 
TEST(exec_changed_keys)
{
	struct signatures new_keys;
	struct signatures old_keys;
	int ret;
	int same = 10;
	int nkeys = NKEYS;
	unsigned long hwcaps = getauxval(AT_HWCAP);

	 
	ASSERT_PAUTH_ENABLED();
	if (!(hwcaps & HWCAP_PACG)) {
		TH_LOG("WARNING: Generic PAUTH not enabled. Skipping generic key checks");
		nkeys = NKEYS - 1;
	}

	for (int i = 0; i < PAC_COLLISION_ATTEMPTS; i++) {
		ret = exec_sign_all(&new_keys, i);
		ASSERT_EQ(0, ret) TH_LOG("failed to run worker");

		if (nkeys == NKEYS)
			sign_all(&old_keys, i);
		else
			sign_specific(&old_keys, i);

		ret = n_same(&old_keys, &new_keys, nkeys);
		if (ret < same)
			same = ret;
	}

	ASSERT_EQ(0, same) TH_LOG("exec() did not change %d keys", same);
}

TEST(context_switch_keep_keys)
{
	int ret;
	struct signatures trash;
	struct signatures before;
	struct signatures after;

	ASSERT_PAUTH_ENABLED();

	sign_specific(&before, ARBITRARY_VALUE);

	 
	ret = exec_sign_all(&trash, ARBITRARY_VALUE);
	ASSERT_EQ(0, ret) TH_LOG("failed to run worker");

	sign_specific(&after, ARBITRARY_VALUE);

	ASSERT_EQ(before.keyia, after.keyia) TH_LOG("keyia changed after context switching");
	ASSERT_EQ(before.keyib, after.keyib) TH_LOG("keyib changed after context switching");
	ASSERT_EQ(before.keyda, after.keyda) TH_LOG("keyda changed after context switching");
	ASSERT_EQ(before.keydb, after.keydb) TH_LOG("keydb changed after context switching");
}

TEST(context_switch_keep_keys_generic)
{
	int ret;
	struct signatures trash;
	size_t before;
	size_t after;

	ASSERT_GENERIC_PAUTH_ENABLED();

	before = keyg_sign(ARBITRARY_VALUE);

	 
	ret = exec_sign_all(&trash, ARBITRARY_VALUE);
	ASSERT_EQ(0, ret) TH_LOG("failed to run worker");

	after = keyg_sign(ARBITRARY_VALUE);

	ASSERT_EQ(before, after) TH_LOG("keyg changed after context switching");
}

TEST_HARNESS_MAIN
