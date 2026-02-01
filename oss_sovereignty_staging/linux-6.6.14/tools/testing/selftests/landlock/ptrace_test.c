
 

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

 
#define YAMA_SCOPE_DISABLED 0
#define YAMA_SCOPE_RELATIONAL 1
#define YAMA_SCOPE_CAPABILITY 2
#define YAMA_SCOPE_NO_ATTACH 3

static void create_domain(struct __test_metadata *const _metadata)
{
	int ruleset_fd;
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_MAKE_BLOCK,
	};

	ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	EXPECT_LE(0, ruleset_fd)
	{
		TH_LOG("Failed to create a ruleset: %s", strerror(errno));
	}
	EXPECT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
	EXPECT_EQ(0, landlock_restrict_self(ruleset_fd, 0));
	EXPECT_EQ(0, close(ruleset_fd));
}

static int test_ptrace_read(const pid_t pid)
{
	static const char path_template[] = "/proc/%d/environ";
	char procenv_path[sizeof(path_template) + 10];
	int procenv_path_size, fd;

	procenv_path_size = snprintf(procenv_path, sizeof(procenv_path),
				     path_template, pid);
	if (procenv_path_size >= sizeof(procenv_path))
		return E2BIG;

	fd = open(procenv_path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return errno;
	 
	if (close(fd) != 0)
		return errno;
	return 0;
}

static int get_yama_ptrace_scope(void)
{
	int ret;
	char buf[2] = {};
	const int fd = open("/proc/sys/kernel/yama/ptrace_scope", O_RDONLY);

	if (fd < 0)
		return 0;

	if (read(fd, buf, 1) < 0) {
		close(fd);
		return -1;
	}

	ret = atoi(buf);
	close(fd);
	return ret;
}

 
FIXTURE(hierarchy) {};
 

FIXTURE_VARIANT(hierarchy)
{
	const bool domain_both;
	const bool domain_parent;
	const bool domain_child;
};

 

 
 
FIXTURE_VARIANT_ADD(hierarchy, allow_without_domain) {
	 
	.domain_both = false,
	.domain_parent = false,
	.domain_child = false,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, allow_with_one_domain) {
	 
	.domain_both = false,
	.domain_parent = false,
	.domain_child = true,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, deny_with_parent_domain) {
	 
	.domain_both = false,
	.domain_parent = true,
	.domain_child = false,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, deny_with_sibling_domain) {
	 
	.domain_both = false,
	.domain_parent = true,
	.domain_child = true,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, allow_sibling_domain) {
	 
	.domain_both = true,
	.domain_parent = false,
	.domain_child = false,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, allow_with_nested_domain) {
	 
	.domain_both = true,
	.domain_parent = false,
	.domain_child = true,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, deny_with_nested_and_parent_domain) {
	 
	.domain_both = true,
	.domain_parent = true,
	.domain_child = false,
};

 
 
FIXTURE_VARIANT_ADD(hierarchy, deny_with_forked_domain) {
	 
	.domain_both = true,
	.domain_parent = true,
	.domain_child = true,
};

FIXTURE_SETUP(hierarchy)
{
}

FIXTURE_TEARDOWN(hierarchy)
{
}

 
TEST_F(hierarchy, trace)
{
	pid_t child, parent;
	int status, err_proc_read;
	int pipe_child[2], pipe_parent[2];
	int yama_ptrace_scope;
	char buf_parent;
	long ret;
	bool can_read_child, can_trace_child, can_read_parent, can_trace_parent;

	yama_ptrace_scope = get_yama_ptrace_scope();
	ASSERT_LE(0, yama_ptrace_scope);

	if (yama_ptrace_scope > YAMA_SCOPE_DISABLED)
		TH_LOG("Incomplete tests due to Yama restrictions (scope %d)",
		       yama_ptrace_scope);

	 
	can_read_child = !variant->domain_parent;

	 
	can_trace_child = can_read_child &&
			  yama_ptrace_scope <= YAMA_SCOPE_RELATIONAL;

	 
	can_read_parent = !variant->domain_child;

	 
	can_trace_parent = can_read_parent &&
			   yama_ptrace_scope <= YAMA_SCOPE_DISABLED;

	 
	drop_caps(_metadata);

	parent = getpid();
	ASSERT_EQ(0, pipe2(pipe_child, O_CLOEXEC));
	ASSERT_EQ(0, pipe2(pipe_parent, O_CLOEXEC));
	if (variant->domain_both) {
		create_domain(_metadata);
		if (!_metadata->passed)
			 
			return;
	}

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		char buf_child;

		ASSERT_EQ(0, close(pipe_parent[1]));
		ASSERT_EQ(0, close(pipe_child[0]));
		if (variant->domain_child)
			create_domain(_metadata);

		 
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));

		 
		err_proc_read = test_ptrace_read(parent);
		if (can_read_parent) {
			EXPECT_EQ(0, err_proc_read);
		} else {
			EXPECT_EQ(EACCES, err_proc_read);
		}

		 
		ret = ptrace(PTRACE_ATTACH, parent, NULL, 0);
		if (can_trace_parent) {
			EXPECT_EQ(0, ret);
		} else {
			EXPECT_EQ(-1, ret);
			EXPECT_EQ(EPERM, errno);
		}
		if (ret == 0) {
			ASSERT_EQ(parent, waitpid(parent, &status, 0));
			ASSERT_EQ(1, WIFSTOPPED(status));
			ASSERT_EQ(0, ptrace(PTRACE_DETACH, parent, NULL, 0));
		}

		 
		ret = ptrace(PTRACE_TRACEME);
		if (can_trace_child) {
			EXPECT_EQ(0, ret);
		} else {
			EXPECT_EQ(-1, ret);
			EXPECT_EQ(EPERM, errno);
		}

		 
		ASSERT_EQ(1, write(pipe_child[1], ".", 1));

		if (can_trace_child) {
			ASSERT_EQ(0, raise(SIGSTOP));
		}

		 
		ASSERT_EQ(1, read(pipe_parent[0], &buf_child, 1));
		_exit(_metadata->passed ? EXIT_SUCCESS : EXIT_FAILURE);
		return;
	}

	ASSERT_EQ(0, close(pipe_child[1]));
	ASSERT_EQ(0, close(pipe_parent[0]));
	if (variant->domain_parent)
		create_domain(_metadata);

	 
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));

	 
	ASSERT_EQ(1, read(pipe_child[0], &buf_parent, 1));

	 
	if (can_trace_child) {
		ASSERT_EQ(child, waitpid(child, &status, 0));
		ASSERT_EQ(1, WIFSTOPPED(status));
		ASSERT_EQ(0, ptrace(PTRACE_DETACH, child, NULL, 0));
	} else {
		 
		EXPECT_EQ(-1, ptrace(PTRACE_DETACH, child, NULL, 0));
		EXPECT_EQ(ESRCH, errno);
	}

	 
	err_proc_read = test_ptrace_read(child);
	if (can_read_child) {
		EXPECT_EQ(0, err_proc_read);
	} else {
		EXPECT_EQ(EACCES, err_proc_read);
	}

	 
	ret = ptrace(PTRACE_ATTACH, child, NULL, 0);
	if (can_trace_child) {
		EXPECT_EQ(0, ret);
	} else {
		EXPECT_EQ(-1, ret);
		EXPECT_EQ(EPERM, errno);
	}

	if (ret == 0) {
		ASSERT_EQ(child, waitpid(child, &status, 0));
		ASSERT_EQ(1, WIFSTOPPED(status));
		ASSERT_EQ(0, ptrace(PTRACE_DETACH, child, NULL, 0));
	}

	 
	ASSERT_EQ(1, write(pipe_parent[1], ".", 1));
	ASSERT_EQ(child, waitpid(child, &status, 0));
	if (WIFSIGNALED(status) || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != EXIT_SUCCESS)
		_metadata->passed = 0;
}

TEST_HARNESS_MAIN
