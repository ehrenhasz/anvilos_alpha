
 
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/auxv.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <asm/sigcontext.h>
#include <asm/ptrace.h>

#include "../../kselftest.h"

 
#ifndef NT_ARM_ZA
#define NT_ARM_ZA 0x40c
#endif

 
#define TEST_VQ_MAX 17

#define EXPECTED_TESTS (((TEST_VQ_MAX - SVE_VQ_MIN) + 1) * 3)

static void fill_buf(char *buf, size_t size)
{
	int i;

	for (i = 0; i < size; i++)
		buf[i] = random();
}

static int do_child(void)
{
	if (ptrace(PTRACE_TRACEME, -1, NULL, NULL))
		ksft_exit_fail_msg("PTRACE_TRACEME", strerror(errno));

	if (raise(SIGSTOP))
		ksft_exit_fail_msg("raise(SIGSTOP)", strerror(errno));

	return EXIT_SUCCESS;
}

static struct user_za_header *get_za(pid_t pid, void **buf, size_t *size)
{
	struct user_za_header *za;
	void *p;
	size_t sz = sizeof(*za);
	struct iovec iov;

	while (1) {
		if (*size < sz) {
			p = realloc(*buf, sz);
			if (!p) {
				errno = ENOMEM;
				goto error;
			}

			*buf = p;
			*size = sz;
		}

		iov.iov_base = *buf;
		iov.iov_len = sz;
		if (ptrace(PTRACE_GETREGSET, pid, NT_ARM_ZA, &iov))
			goto error;

		za = *buf;
		if (za->size <= sz)
			break;

		sz = za->size;
	}

	return za;

error:
	return NULL;
}

static int set_za(pid_t pid, const struct user_za_header *za)
{
	struct iovec iov;

	iov.iov_base = (void *)za;
	iov.iov_len = za->size;
	return ptrace(PTRACE_SETREGSET, pid, NT_ARM_ZA, &iov);
}

 
static void ptrace_set_get_vl(pid_t child, unsigned int vl, bool *supported)
{
	struct user_za_header za;
	struct user_za_header *new_za = NULL;
	size_t new_za_size = 0;
	int ret, prctl_vl;

	*supported = false;

	 
	prctl_vl = prctl(PR_SME_SET_VL, vl);
	if (prctl_vl == -1)
		ksft_exit_fail_msg("prctl(PR_SME_SET_VL) failed: %s (%d)\n",
				   strerror(errno), errno);

	 
	*supported = (prctl_vl == vl);

	 
	memset(&za, 0, sizeof(za));
	za.size = sizeof(za);
	za.vl = vl;
	ret = set_za(child, &za);
	if (ret != 0) {
		ksft_test_result_fail("Failed to set VL %u\n", vl);
		return;
	}

	 
	if (!get_za(child, (void **)&new_za, &new_za_size)) {
		ksft_test_result_fail("Failed to read VL %u\n", vl);
		return;
	}

	ksft_test_result(new_za->vl = prctl_vl, "Set VL %u\n", vl);

	free(new_za);
}

 
static void ptrace_set_no_data(pid_t child, unsigned int vl)
{
	void *read_buf = NULL;
	struct user_za_header write_za;
	struct user_za_header *read_za;
	size_t read_za_size = 0;
	int ret;

	 
	memset(&write_za, 0, sizeof(write_za));
	write_za.size = ZA_PT_ZA_OFFSET;
	write_za.vl = vl;

	ret = set_za(child, &write_za);
	if (ret != 0) {
		ksft_test_result_fail("Failed to set VL %u no data\n", vl);
		return;
	}

	 
	if (!get_za(child, (void **)&read_buf, &read_za_size)) {
		ksft_test_result_fail("Failed to read VL %u no data\n", vl);
		return;
	}
	read_za = read_buf;

	 
	if (read_za->size < write_za.size) {
		ksft_test_result_fail("VL %u wrote %d bytes, only read %d\n",
				      vl, write_za.size, read_za->size);
		goto out_read;
	}

	ksft_test_result(read_za->size == write_za.size,
			 "Disabled ZA for VL %u\n", vl);

out_read:
	free(read_buf);
}

 
static void ptrace_set_get_data(pid_t child, unsigned int vl)
{
	void *write_buf;
	void *read_buf = NULL;
	struct user_za_header *write_za;
	struct user_za_header *read_za;
	size_t read_za_size = 0;
	unsigned int vq = sve_vq_from_vl(vl);
	int ret;
	size_t data_size;

	data_size = ZA_PT_SIZE(vq);
	write_buf = malloc(data_size);
	if (!write_buf) {
		ksft_test_result_fail("Error allocating %d byte buffer for VL %u\n",
				      data_size, vl);
		return;
	}
	write_za = write_buf;

	 
	memset(write_za, 0, data_size);
	write_za->size = data_size;
	write_za->vl = vl;

	fill_buf(write_buf + ZA_PT_ZA_OFFSET, ZA_PT_ZA_SIZE(vq));

	ret = set_za(child, write_za);
	if (ret != 0) {
		ksft_test_result_fail("Failed to set VL %u data\n", vl);
		goto out;
	}

	 
	if (!get_za(child, (void **)&read_buf, &read_za_size)) {
		ksft_test_result_fail("Failed to read VL %u data\n", vl);
		goto out;
	}
	read_za = read_buf;

	 
	if (read_za->size < write_za->size) {
		ksft_test_result_fail("VL %u wrote %d bytes, only read %d\n",
				      vl, write_za->size, read_za->size);
		goto out_read;
	}

	ksft_test_result(memcmp(write_buf + ZA_PT_ZA_OFFSET,
				read_buf + ZA_PT_ZA_OFFSET,
				ZA_PT_ZA_SIZE(vq)) == 0,
			 "Data match for VL %u\n", vl);

out_read:
	free(read_buf);
out:
	free(write_buf);
}

static int do_parent(pid_t child)
{
	int ret = EXIT_FAILURE;
	pid_t pid;
	int status;
	siginfo_t si;
	unsigned int vq, vl;
	bool vl_supported;

	 
	while (1) {
		int sig;

		pid = wait(&status);
		if (pid == -1) {
			perror("wait");
			goto error;
		}

		 
		if (pid != child)
			continue;

		if (WIFEXITED(status) || WIFSIGNALED(status))
			ksft_exit_fail_msg("Child died unexpectedly\n");

		if (!WIFSTOPPED(status))
			goto error;

		sig = WSTOPSIG(status);

		if (ptrace(PTRACE_GETSIGINFO, pid, NULL, &si)) {
			if (errno == ESRCH)
				goto disappeared;

			if (errno == EINVAL) {
				sig = 0;  
				goto cont;
			}

			ksft_test_result_fail("PTRACE_GETSIGINFO: %s\n",
					      strerror(errno));
			goto error;
		}

		if (sig == SIGSTOP && si.si_code == SI_TKILL &&
		    si.si_pid == pid)
			break;

	cont:
		if (ptrace(PTRACE_CONT, pid, NULL, sig)) {
			if (errno == ESRCH)
				goto disappeared;

			ksft_test_result_fail("PTRACE_CONT: %s\n",
					      strerror(errno));
			goto error;
		}
	}

	ksft_print_msg("Parent is %d, child is %d\n", getpid(), child);

	 
	for (vq = SVE_VQ_MIN; vq <= TEST_VQ_MAX; vq++) {
		vl = sve_vl_from_vq(vq);

		 
		ptrace_set_get_vl(child, vl, &vl_supported);

		 
		if (vl_supported) {
			ptrace_set_no_data(child, vl);
			ptrace_set_get_data(child, vl);
		} else {
			ksft_test_result_skip("Disabled ZA for VL %u\n", vl);
			ksft_test_result_skip("Get and set data for VL %u\n",
					      vl);
		}
	}

	ret = EXIT_SUCCESS;

error:
	kill(child, SIGKILL);

disappeared:
	return ret;
}

int main(void)
{
	int ret = EXIT_SUCCESS;
	pid_t child;

	srandom(getpid());

	ksft_print_header();

	if (!(getauxval(AT_HWCAP2) & HWCAP2_SME)) {
		ksft_set_plan(1);
		ksft_exit_skip("SME not available\n");
	}

	ksft_set_plan(EXPECTED_TESTS);

	child = fork();
	if (!child)
		return do_child();

	if (do_parent(child))
		ret = EXIT_FAILURE;

	ksft_print_cnts();

	return ret;
}
