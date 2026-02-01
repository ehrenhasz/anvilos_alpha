
 

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/landlock.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "common.h"

#ifndef O_PATH
#define O_PATH 010000000
#endif

TEST(inconsistent_attr)
{
	const long page_size = sysconf(_SC_PAGESIZE);
	char *const buf = malloc(page_size + 1);
	struct landlock_ruleset_attr *const ruleset_attr = (void *)buf;

	ASSERT_NE(NULL, buf);

	 
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, 0, 0));
	 
	ASSERT_EQ(EINVAL, errno);
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, 1, 0));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, 7, 0));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(NULL, 1, 0));
	 
	ASSERT_EQ(EFAULT, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(
			      NULL, sizeof(struct landlock_ruleset_attr), 0));
	ASSERT_EQ(EFAULT, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, page_size + 1, 0));
	ASSERT_EQ(E2BIG, errno);

	 
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, 8, 0));
	ASSERT_EQ(ENOMSG, errno);
	ASSERT_EQ(-1, landlock_create_ruleset(
			      ruleset_attr,
			      sizeof(struct landlock_ruleset_attr), 0));
	ASSERT_EQ(ENOMSG, errno);
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, page_size, 0));
	ASSERT_EQ(ENOMSG, errno);

	 
	buf[page_size - 2] = '.';
	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, page_size, 0));
	ASSERT_EQ(E2BIG, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(ruleset_attr, page_size + 1, 0));
	ASSERT_EQ(E2BIG, errno);

	free(buf);
}

TEST(abi_version)
{
	const struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE,
	};
	ASSERT_EQ(3, landlock_create_ruleset(NULL, 0,
					     LANDLOCK_CREATE_RULESET_VERSION));

	ASSERT_EQ(-1, landlock_create_ruleset(&ruleset_attr, 0,
					      LANDLOCK_CREATE_RULESET_VERSION));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(NULL, sizeof(ruleset_attr),
					      LANDLOCK_CREATE_RULESET_VERSION));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1,
		  landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr),
					  LANDLOCK_CREATE_RULESET_VERSION));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(NULL, 0,
					      LANDLOCK_CREATE_RULESET_VERSION |
						      1 << 31));
	ASSERT_EQ(EINVAL, errno);
}

 
TEST(create_ruleset_checks_ordering)
{
	const int last_flag = LANDLOCK_CREATE_RULESET_VERSION;
	const int invalid_flag = last_flag << 1;
	int ruleset_fd;
	const struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE,
	};

	 
	ASSERT_EQ(-1, landlock_create_ruleset(NULL, 0, invalid_flag));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(&ruleset_attr, 0, invalid_flag));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1, landlock_create_ruleset(NULL, sizeof(ruleset_attr),
					      invalid_flag));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(-1,
		  landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr),
					  invalid_flag));
	ASSERT_EQ(EINVAL, errno);

	 
	ASSERT_EQ(-1, landlock_create_ruleset(&ruleset_attr, -1, 0));
	ASSERT_EQ(E2BIG, errno);

	 
	ASSERT_EQ(-1, landlock_create_ruleset(&ruleset_attr, 0, 0));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_EQ(-1, landlock_create_ruleset(&ruleset_attr, 1, 0));
	ASSERT_EQ(EINVAL, errno);

	 
	ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	ASSERT_LE(0, ruleset_fd);
	ASSERT_EQ(0, close(ruleset_fd));
}

 
TEST(add_rule_checks_ordering)
{
	const struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_EXECUTE,
	};
	struct landlock_path_beneath_attr path_beneath_attr = {
		.allowed_access = LANDLOCK_ACCESS_FS_EXECUTE,
		.parent_fd = -1,
	};
	const int ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);

	ASSERT_LE(0, ruleset_fd);

	 
	ASSERT_EQ(-1, landlock_add_rule(-1, 0, NULL, 1));
	ASSERT_EQ(EINVAL, errno);

	 
	ASSERT_EQ(-1, landlock_add_rule(-1, 0, NULL, 0));
	ASSERT_EQ(EBADF, errno);

	 
	ASSERT_EQ(-1, landlock_add_rule(ruleset_fd, 0, NULL, 0));
	ASSERT_EQ(EINVAL, errno);

	 
	ASSERT_EQ(-1, landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
					NULL, 0));
	ASSERT_EQ(EFAULT, errno);

	 
	ASSERT_EQ(-1, landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
					&path_beneath_attr, 0));
	ASSERT_EQ(EBADF, errno);

	 
	path_beneath_attr.parent_fd =
		open("/tmp", O_PATH | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
	ASSERT_LE(0, path_beneath_attr.parent_fd);
	ASSERT_EQ(0, landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
				       &path_beneath_attr, 0));
	ASSERT_EQ(0, close(path_beneath_attr.parent_fd));
	ASSERT_EQ(0, close(ruleset_fd));
}

 
TEST(restrict_self_checks_ordering)
{
	const struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_EXECUTE,
	};
	struct landlock_path_beneath_attr path_beneath_attr = {
		.allowed_access = LANDLOCK_ACCESS_FS_EXECUTE,
		.parent_fd = -1,
	};
	const int ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);

	ASSERT_LE(0, ruleset_fd);
	path_beneath_attr.parent_fd =
		open("/tmp", O_PATH | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
	ASSERT_LE(0, path_beneath_attr.parent_fd);
	ASSERT_EQ(0, landlock_add_rule(ruleset_fd, LANDLOCK_RULE_PATH_BENEATH,
				       &path_beneath_attr, 0));
	ASSERT_EQ(0, close(path_beneath_attr.parent_fd));

	 
	drop_caps(_metadata);
	ASSERT_EQ(-1, landlock_restrict_self(-1, -1));
	ASSERT_EQ(EPERM, errno);
	ASSERT_EQ(-1, landlock_restrict_self(-1, 0));
	ASSERT_EQ(EPERM, errno);
	ASSERT_EQ(-1, landlock_restrict_self(ruleset_fd, 0));
	ASSERT_EQ(EPERM, errno);

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));

	 
	ASSERT_EQ(-1, landlock_restrict_self(-1, -1));
	ASSERT_EQ(EINVAL, errno);

	 
	ASSERT_EQ(-1, landlock_restrict_self(-1, 0));
	ASSERT_EQ(EBADF, errno);

	 
	ASSERT_EQ(0, landlock_restrict_self(ruleset_fd, 0));
	ASSERT_EQ(0, close(ruleset_fd));
}

TEST(ruleset_fd_io)
{
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_READ_FILE,
	};
	int ruleset_fd;
	char buf;

	drop_caps(_metadata);
	ruleset_fd =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	ASSERT_LE(0, ruleset_fd);

	ASSERT_EQ(-1, write(ruleset_fd, ".", 1));
	ASSERT_EQ(EINVAL, errno);
	ASSERT_EQ(-1, read(ruleset_fd, &buf, 1));
	ASSERT_EQ(EINVAL, errno);

	ASSERT_EQ(0, close(ruleset_fd));
}

 
TEST(ruleset_fd_transfer)
{
	struct landlock_ruleset_attr ruleset_attr = {
		.handled_access_fs = LANDLOCK_ACCESS_FS_READ_DIR,
	};
	struct landlock_path_beneath_attr path_beneath_attr = {
		.allowed_access = LANDLOCK_ACCESS_FS_READ_DIR,
	};
	int ruleset_fd_tx, dir_fd;
	int socket_fds[2];
	pid_t child;
	int status;

	drop_caps(_metadata);

	 
	ruleset_fd_tx =
		landlock_create_ruleset(&ruleset_attr, sizeof(ruleset_attr), 0);
	ASSERT_LE(0, ruleset_fd_tx);
	path_beneath_attr.parent_fd =
		open("/tmp", O_PATH | O_NOFOLLOW | O_DIRECTORY | O_CLOEXEC);
	ASSERT_LE(0, path_beneath_attr.parent_fd);
	ASSERT_EQ(0,
		  landlock_add_rule(ruleset_fd_tx, LANDLOCK_RULE_PATH_BENEATH,
				    &path_beneath_attr, 0));
	ASSERT_EQ(0, close(path_beneath_attr.parent_fd));

	 
	ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0,
				socket_fds));
	ASSERT_EQ(0, send_fd(socket_fds[0], ruleset_fd_tx));
	ASSERT_EQ(0, close(socket_fds[0]));
	ASSERT_EQ(0, close(ruleset_fd_tx));

	child = fork();
	ASSERT_LE(0, child);
	if (child == 0) {
		const int ruleset_fd_rx = recv_fd(socket_fds[1]);

		ASSERT_LE(0, ruleset_fd_rx);
		ASSERT_EQ(0, close(socket_fds[1]));

		 
		ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
		ASSERT_EQ(0, landlock_restrict_self(ruleset_fd_rx, 0));
		ASSERT_EQ(0, close(ruleset_fd_rx));

		 
		ASSERT_EQ(-1, open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
		ASSERT_EQ(EACCES, errno);
		dir_fd = open("/tmp", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		ASSERT_LE(0, dir_fd);
		ASSERT_EQ(0, close(dir_fd));
		_exit(_metadata->passed ? EXIT_SUCCESS : EXIT_FAILURE);
		return;
	}

	ASSERT_EQ(0, close(socket_fds[1]));

	 
	dir_fd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	ASSERT_LE(0, dir_fd);
	ASSERT_EQ(0, close(dir_fd));
	dir_fd = open("/tmp", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	ASSERT_LE(0, dir_fd);
	ASSERT_EQ(0, close(dir_fd));

	ASSERT_EQ(child, waitpid(child, &status, 0));
	ASSERT_EQ(1, WIFEXITED(status));
	ASSERT_EQ(EXIT_SUCCESS, WEXITSTATUS(status));
}

TEST_HARNESS_MAIN
