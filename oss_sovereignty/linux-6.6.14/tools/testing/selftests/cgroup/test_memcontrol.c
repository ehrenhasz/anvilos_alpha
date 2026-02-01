 
#define _GNU_SOURCE

#include <linux/limits.h>
#include <linux/oom.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <sys/mman.h>

#include "../kselftest.h"
#include "cgroup_util.h"

static bool has_localevents;
static bool has_recursiveprot;

 
static int test_memcg_subtree_control(const char *root)
{
	char *parent, *child, *parent2 = NULL, *child2 = NULL;
	int ret = KSFT_FAIL;
	char buf[PAGE_SIZE];

	 
	parent = cg_name(root, "memcg_test_0");
	child = cg_name(root, "memcg_test_0/memcg_test_1");
	if (!parent || !child)
		goto cleanup_free;

	if (cg_create(parent))
		goto cleanup_free;

	if (cg_write(parent, "cgroup.subtree_control", "+memory"))
		goto cleanup_parent;

	if (cg_create(child))
		goto cleanup_parent;

	if (cg_read_strstr(child, "cgroup.controllers", "memory"))
		goto cleanup_child;

	 
	parent2 = cg_name(root, "memcg_test_1");
	child2 = cg_name(root, "memcg_test_1/memcg_test_1");
	if (!parent2 || !child2)
		goto cleanup_free2;

	if (cg_create(parent2))
		goto cleanup_free2;

	if (cg_create(child2))
		goto cleanup_parent2;

	if (cg_read(child2, "cgroup.controllers", buf, sizeof(buf)))
		goto cleanup_all;

	if (!cg_read_strstr(child2, "cgroup.controllers", "memory"))
		goto cleanup_all;

	ret = KSFT_PASS;

cleanup_all:
	cg_destroy(child2);
cleanup_parent2:
	cg_destroy(parent2);
cleanup_free2:
	free(parent2);
	free(child2);
cleanup_child:
	cg_destroy(child);
cleanup_parent:
	cg_destroy(parent);
cleanup_free:
	free(parent);
	free(child);

	return ret;
}

static int alloc_anon_50M_check(const char *cgroup, void *arg)
{
	size_t size = MB(50);
	char *buf, *ptr;
	long anon, current;
	int ret = -1;

	buf = malloc(size);
	if (buf == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return -1;
	}

	for (ptr = buf; ptr < buf + size; ptr += PAGE_SIZE)
		*ptr = 0;

	current = cg_read_long(cgroup, "memory.current");
	if (current < size)
		goto cleanup;

	if (!values_close(size, current, 3))
		goto cleanup;

	anon = cg_read_key_long(cgroup, "memory.stat", "anon ");
	if (anon < 0)
		goto cleanup;

	if (!values_close(anon, current, 3))
		goto cleanup;

	ret = 0;
cleanup:
	free(buf);
	return ret;
}

static int alloc_pagecache_50M_check(const char *cgroup, void *arg)
{
	size_t size = MB(50);
	int ret = -1;
	long current, file;
	int fd;

	fd = get_temp_fd();
	if (fd < 0)
		return -1;

	if (alloc_pagecache(fd, size))
		goto cleanup;

	current = cg_read_long(cgroup, "memory.current");
	if (current < size)
		goto cleanup;

	file = cg_read_key_long(cgroup, "memory.stat", "file ");
	if (file < 0)
		goto cleanup;

	if (!values_close(file, current, 10))
		goto cleanup;

	ret = 0;

cleanup:
	close(fd);
	return ret;
}

 
static int test_memcg_current(const char *root)
{
	int ret = KSFT_FAIL;
	long current;
	char *memcg;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	current = cg_read_long(memcg, "memory.current");
	if (current != 0)
		goto cleanup;

	if (cg_run(memcg, alloc_anon_50M_check, NULL))
		goto cleanup;

	if (cg_run(memcg, alloc_pagecache_50M_check, NULL))
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

static int alloc_pagecache_50M_noexit(const char *cgroup, void *arg)
{
	int fd = (long)arg;
	int ppid = getppid();

	if (alloc_pagecache(fd, MB(50)))
		return -1;

	while (getppid() == ppid)
		sleep(1);

	return 0;
}

static int alloc_anon_noexit(const char *cgroup, void *arg)
{
	int ppid = getppid();
	size_t size = (unsigned long)arg;
	char *buf, *ptr;

	buf = malloc(size);
	if (buf == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return -1;
	}

	for (ptr = buf; ptr < buf + size; ptr += PAGE_SIZE)
		*ptr = 0;

	while (getppid() == ppid)
		sleep(1);

	free(buf);
	return 0;
}

 
static int cg_test_proc_killed(const char *cgroup)
{
	int limit;

	for (limit = 10; limit > 0; limit--) {
		if (cg_read_strcmp(cgroup, "cgroup.procs", "") == 0)
			return 0;

		usleep(100000);
	}
	return -1;
}

static bool reclaim_until(const char *memcg, long goal);

 
static int test_memcg_protection(const char *root, bool min)
{
	int ret = KSFT_FAIL, rc;
	char *parent[3] = {NULL};
	char *children[4] = {NULL};
	const char *attribute = min ? "memory.min" : "memory.low";
	long c[4];
	long current;
	int i, attempts;
	int fd;

	fd = get_temp_fd();
	if (fd < 0)
		goto cleanup;

	parent[0] = cg_name(root, "memcg_test_0");
	if (!parent[0])
		goto cleanup;

	parent[1] = cg_name(parent[0], "memcg_test_1");
	if (!parent[1])
		goto cleanup;

	parent[2] = cg_name(parent[0], "memcg_test_2");
	if (!parent[2])
		goto cleanup;

	if (cg_create(parent[0]))
		goto cleanup;

	if (cg_read_long(parent[0], attribute)) {
		 
		if (min)
			ret = KSFT_SKIP;
		goto cleanup;
	}

	if (cg_write(parent[0], "cgroup.subtree_control", "+memory"))
		goto cleanup;

	if (cg_write(parent[0], "memory.max", "200M"))
		goto cleanup;

	if (cg_write(parent[0], "memory.swap.max", "0"))
		goto cleanup;

	if (cg_create(parent[1]))
		goto cleanup;

	if (cg_write(parent[1], "cgroup.subtree_control", "+memory"))
		goto cleanup;

	if (cg_create(parent[2]))
		goto cleanup;

	for (i = 0; i < ARRAY_SIZE(children); i++) {
		children[i] = cg_name_indexed(parent[1], "child_memcg", i);
		if (!children[i])
			goto cleanup;

		if (cg_create(children[i]))
			goto cleanup;

		if (i > 2)
			continue;

		cg_run_nowait(children[i], alloc_pagecache_50M_noexit,
			      (void *)(long)fd);
	}

	if (cg_write(parent[1],   attribute, "50M"))
		goto cleanup;
	if (cg_write(children[0], attribute, "75M"))
		goto cleanup;
	if (cg_write(children[1], attribute, "25M"))
		goto cleanup;
	if (cg_write(children[2], attribute, "0"))
		goto cleanup;
	if (cg_write(children[3], attribute, "500M"))
		goto cleanup;

	attempts = 0;
	while (!values_close(cg_read_long(parent[1], "memory.current"),
			     MB(150), 3)) {
		if (attempts++ > 5)
			break;
		sleep(1);
	}

	if (cg_run(parent[2], alloc_anon, (void *)MB(148)))
		goto cleanup;

	if (!values_close(cg_read_long(parent[1], "memory.current"), MB(50), 3))
		goto cleanup;

	for (i = 0; i < ARRAY_SIZE(children); i++)
		c[i] = cg_read_long(children[i], "memory.current");

	if (!values_close(c[0], MB(29), 10))
		goto cleanup;

	if (!values_close(c[1], MB(21), 10))
		goto cleanup;

	if (c[3] != 0)
		goto cleanup;

	rc = cg_run(parent[2], alloc_anon, (void *)MB(170));
	if (min && !rc)
		goto cleanup;
	else if (!min && rc) {
		fprintf(stderr,
			"memory.low prevents from allocating anon memory\n");
		goto cleanup;
	}

	current = min ? MB(50) : MB(30);
	if (!values_close(cg_read_long(parent[1], "memory.current"), current, 3))
		goto cleanup;

	if (!reclaim_until(children[0], MB(10)))
		goto cleanup;

	if (min) {
		ret = KSFT_PASS;
		goto cleanup;
	}

	for (i = 0; i < ARRAY_SIZE(children); i++) {
		int no_low_events_index = 1;
		long low, oom;

		oom = cg_read_key_long(children[i], "memory.events", "oom ");
		low = cg_read_key_long(children[i], "memory.events", "low ");

		if (oom)
			goto cleanup;
		if (i <= no_low_events_index && low <= 0)
			goto cleanup;
		if (i > no_low_events_index && low)
			goto cleanup;

	}

	ret = KSFT_PASS;

cleanup:
	for (i = ARRAY_SIZE(children) - 1; i >= 0; i--) {
		if (!children[i])
			continue;

		cg_destroy(children[i]);
		free(children[i]);
	}

	for (i = ARRAY_SIZE(parent) - 1; i >= 0; i--) {
		if (!parent[i])
			continue;

		cg_destroy(parent[i]);
		free(parent[i]);
	}
	close(fd);
	return ret;
}

static int test_memcg_min(const char *root)
{
	return test_memcg_protection(root, true);
}

static int test_memcg_low(const char *root)
{
	return test_memcg_protection(root, false);
}

static int alloc_pagecache_max_30M(const char *cgroup, void *arg)
{
	size_t size = MB(50);
	int ret = -1;
	long current, high, max;
	int fd;

	high = cg_read_long(cgroup, "memory.high");
	max = cg_read_long(cgroup, "memory.max");
	if (high != MB(30) && max != MB(30))
		return -1;

	fd = get_temp_fd();
	if (fd < 0)
		return -1;

	if (alloc_pagecache(fd, size))
		goto cleanup;

	current = cg_read_long(cgroup, "memory.current");
	if (!values_close(current, MB(30), 5))
		goto cleanup;

	ret = 0;

cleanup:
	close(fd);
	return ret;

}

 
static int test_memcg_high(const char *root)
{
	int ret = KSFT_FAIL;
	char *memcg;
	long high;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	if (cg_read_strcmp(memcg, "memory.high", "max\n"))
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(memcg, "memory.high", "30M"))
		goto cleanup;

	if (cg_run(memcg, alloc_anon, (void *)MB(31)))
		goto cleanup;

	if (!cg_run(memcg, alloc_pagecache_50M_check, NULL))
		goto cleanup;

	if (cg_run(memcg, alloc_pagecache_max_30M, NULL))
		goto cleanup;

	high = cg_read_key_long(memcg, "memory.events", "high ");
	if (high <= 0)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

static int alloc_anon_mlock(const char *cgroup, void *arg)
{
	size_t size = (size_t)arg;
	void *buf;

	buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON,
		   0, 0);
	if (buf == MAP_FAILED)
		return -1;

	mlock(buf, size);
	munmap(buf, size);
	return 0;
}

 
static int test_memcg_high_sync(const char *root)
{
	int ret = KSFT_FAIL, pid, fd = -1;
	char *memcg;
	long pre_high, pre_max;
	long post_high, post_max;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	pre_high = cg_read_key_long(memcg, "memory.events", "high ");
	pre_max = cg_read_key_long(memcg, "memory.events", "max ");
	if (pre_high < 0 || pre_max < 0)
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(memcg, "memory.high", "30M"))
		goto cleanup;

	if (cg_write(memcg, "memory.max", "140M"))
		goto cleanup;

	fd = memcg_prepare_for_wait(memcg);
	if (fd < 0)
		goto cleanup;

	pid = cg_run_nowait(memcg, alloc_anon_mlock, (void *)MB(200));
	if (pid < 0)
		goto cleanup;

	cg_wait_for(fd);

	post_high = cg_read_key_long(memcg, "memory.events", "high ");
	post_max = cg_read_key_long(memcg, "memory.events", "max ");
	if (post_high < 0 || post_max < 0)
		goto cleanup;

	if (pre_high == post_high || pre_max != post_max)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	if (fd >= 0)
		close(fd);
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

 
static int test_memcg_max(const char *root)
{
	int ret = KSFT_FAIL;
	char *memcg;
	long current, max;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	if (cg_read_strcmp(memcg, "memory.max", "max\n"))
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(memcg, "memory.max", "30M"))
		goto cleanup;

	 
	if (!cg_run(memcg, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_run(memcg, alloc_pagecache_max_30M, NULL))
		goto cleanup;

	current = cg_read_long(memcg, "memory.current");
	if (current > MB(30) || !current)
		goto cleanup;

	max = cg_read_key_long(memcg, "memory.events", "max ");
	if (max <= 0)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

 
static bool reclaim_until(const char *memcg, long goal)
{
	char buf[64];
	int retries, err;
	long current, to_reclaim;
	bool reclaimed = false;

	for (retries = 5; retries > 0; retries--) {
		current = cg_read_long(memcg, "memory.current");

		if (current < goal || values_close(current, goal, 3))
			break;
		 
		else if (reclaimed)
			return false;

		to_reclaim = current - goal;
		snprintf(buf, sizeof(buf), "%ld", to_reclaim);
		err = cg_write(memcg, "memory.reclaim", buf);
		if (!err)
			reclaimed = true;
		else if (err != -EAGAIN)
			return false;
	}
	return reclaimed;
}

 
static int test_memcg_reclaim(const char *root)
{
	int ret = KSFT_FAIL, fd, retries;
	char *memcg;
	long current, expected_usage;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	current = cg_read_long(memcg, "memory.current");
	if (current != 0)
		goto cleanup;

	fd = get_temp_fd();
	if (fd < 0)
		goto cleanup;

	cg_run_nowait(memcg, alloc_pagecache_50M_noexit, (void *)(long)fd);

	 
	if (is_swap_enabled()) {
		cg_run_nowait(memcg, alloc_anon_noexit, (void *) MB(50));
		expected_usage = MB(100);
	} else
		expected_usage = MB(50);

	 
	retries = 5;
	while (!values_close(cg_read_long(memcg, "memory.current"),
			    expected_usage, 10)) {
		if (retries--) {
			sleep(1);
			continue;
		} else {
			fprintf(stderr,
				"failed to allocate %ld for memcg reclaim test\n",
				expected_usage);
			goto cleanup;
		}
	}

	 
	if (!reclaim_until(memcg, MB(30)))
		goto cleanup;

	ret = KSFT_PASS;
cleanup:
	cg_destroy(memcg);
	free(memcg);
	close(fd);

	return ret;
}

static int alloc_anon_50M_check_swap(const char *cgroup, void *arg)
{
	long mem_max = (long)arg;
	size_t size = MB(50);
	char *buf, *ptr;
	long mem_current, swap_current;
	int ret = -1;

	buf = malloc(size);
	if (buf == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return -1;
	}

	for (ptr = buf; ptr < buf + size; ptr += PAGE_SIZE)
		*ptr = 0;

	mem_current = cg_read_long(cgroup, "memory.current");
	if (!mem_current || !values_close(mem_current, mem_max, 3))
		goto cleanup;

	swap_current = cg_read_long(cgroup, "memory.swap.current");
	if (!swap_current ||
	    !values_close(mem_current + swap_current, size, 3))
		goto cleanup;

	ret = 0;
cleanup:
	free(buf);
	return ret;
}

 
static int test_memcg_swap_max(const char *root)
{
	int ret = KSFT_FAIL;
	char *memcg;
	long max;

	if (!is_swap_enabled())
		return KSFT_SKIP;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	if (cg_read_long(memcg, "memory.swap.current")) {
		ret = KSFT_SKIP;
		goto cleanup;
	}

	if (cg_read_strcmp(memcg, "memory.max", "max\n"))
		goto cleanup;

	if (cg_read_strcmp(memcg, "memory.swap.max", "max\n"))
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "30M"))
		goto cleanup;

	if (cg_write(memcg, "memory.max", "30M"))
		goto cleanup;

	 
	if (!cg_run(memcg, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.events", "oom ") != 1)
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.events", "oom_kill ") != 1)
		goto cleanup;

	if (cg_run(memcg, alloc_anon_50M_check_swap, (void *)MB(30)))
		goto cleanup;

	max = cg_read_key_long(memcg, "memory.events", "max ");
	if (max <= 0)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

 
static int test_memcg_oom_events(const char *root)
{
	int ret = KSFT_FAIL;
	char *memcg;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	if (cg_write(memcg, "memory.max", "30M"))
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "0"))
		goto cleanup;

	if (!cg_run(memcg, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_read_strcmp(memcg, "cgroup.procs", ""))
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.events", "oom ") != 1)
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.events", "oom_kill ") != 1)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

struct tcp_server_args {
	unsigned short port;
	int ctl[2];
};

static int tcp_server(const char *cgroup, void *arg)
{
	struct tcp_server_args *srv_args = arg;
	struct sockaddr_in6 saddr = { 0 };
	socklen_t slen = sizeof(saddr);
	int sk, client_sk, ctl_fd, yes = 1, ret = -1;

	close(srv_args->ctl[0]);
	ctl_fd = srv_args->ctl[1];

	saddr.sin6_family = AF_INET6;
	saddr.sin6_addr = in6addr_any;
	saddr.sin6_port = htons(srv_args->port);

	sk = socket(AF_INET6, SOCK_STREAM, 0);
	if (sk < 0)
		return ret;

	if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
		goto cleanup;

	if (bind(sk, (struct sockaddr *)&saddr, slen)) {
		write(ctl_fd, &errno, sizeof(errno));
		goto cleanup;
	}

	if (listen(sk, 1))
		goto cleanup;

	ret = 0;
	if (write(ctl_fd, &ret, sizeof(ret)) != sizeof(ret)) {
		ret = -1;
		goto cleanup;
	}

	client_sk = accept(sk, NULL, NULL);
	if (client_sk < 0)
		goto cleanup;

	ret = -1;
	for (;;) {
		uint8_t buf[0x100000];

		if (write(client_sk, buf, sizeof(buf)) <= 0) {
			if (errno == ECONNRESET)
				ret = 0;
			break;
		}
	}

	close(client_sk);

cleanup:
	close(sk);
	return ret;
}

static int tcp_client(const char *cgroup, unsigned short port)
{
	const char server[] = "localhost";
	struct addrinfo *ai;
	char servport[6];
	int retries = 0x10;  
	int sk, ret;
	long allocated;

	allocated = cg_read_long(cgroup, "memory.current");
	snprintf(servport, sizeof(servport), "%hd", port);
	ret = getaddrinfo(server, servport, NULL, &ai);
	if (ret)
		return ret;

	sk = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sk < 0)
		goto free_ainfo;

	ret = connect(sk, ai->ai_addr, ai->ai_addrlen);
	if (ret < 0)
		goto close_sk;

	ret = KSFT_FAIL;
	while (retries--) {
		uint8_t buf[0x100000];
		long current, sock;

		if (read(sk, buf, sizeof(buf)) <= 0)
			goto close_sk;

		current = cg_read_long(cgroup, "memory.current");
		sock = cg_read_key_long(cgroup, "memory.stat", "sock ");

		if (current < 0 || sock < 0)
			goto close_sk;

		 
		if (values_close(current - allocated, sock, 10)) {
			ret = KSFT_PASS;
			break;
		}
	}

close_sk:
	close(sk);
free_ainfo:
	freeaddrinfo(ai);
	return ret;
}

 
static int test_memcg_sock(const char *root)
{
	int bind_retries = 5, ret = KSFT_FAIL, pid, err;
	unsigned short port;
	char *memcg;

	memcg = cg_name(root, "memcg_test");
	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	while (bind_retries--) {
		struct tcp_server_args args;

		if (pipe(args.ctl))
			goto cleanup;

		port = args.port = 1000 + rand() % 60000;

		pid = cg_run_nowait(memcg, tcp_server, &args);
		if (pid < 0)
			goto cleanup;

		close(args.ctl[1]);
		if (read(args.ctl[0], &err, sizeof(err)) != sizeof(err))
			goto cleanup;
		close(args.ctl[0]);

		if (!err)
			break;
		if (err != EADDRINUSE)
			goto cleanup;

		waitpid(pid, NULL, 0);
	}

	if (err == EADDRINUSE) {
		ret = KSFT_SKIP;
		goto cleanup;
	}

	if (tcp_client(memcg, port) != KSFT_PASS)
		goto cleanup;

	waitpid(pid, &err, 0);
	if (WEXITSTATUS(err))
		goto cleanup;

	if (cg_read_long(memcg, "memory.current") < 0)
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.stat", "sock "))
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	cg_destroy(memcg);
	free(memcg);

	return ret;
}

 
static int test_memcg_oom_group_leaf_events(const char *root)
{
	int ret = KSFT_FAIL;
	char *parent, *child;
	long parent_oom_events;

	parent = cg_name(root, "memcg_test_0");
	child = cg_name(root, "memcg_test_0/memcg_test_1");

	if (!parent || !child)
		goto cleanup;

	if (cg_create(parent))
		goto cleanup;

	if (cg_create(child))
		goto cleanup;

	if (cg_write(parent, "cgroup.subtree_control", "+memory"))
		goto cleanup;

	if (cg_write(child, "memory.max", "50M"))
		goto cleanup;

	if (cg_write(child, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(child, "memory.oom.group", "1"))
		goto cleanup;

	cg_run_nowait(parent, alloc_anon_noexit, (void *) MB(60));
	cg_run_nowait(child, alloc_anon_noexit, (void *) MB(1));
	cg_run_nowait(child, alloc_anon_noexit, (void *) MB(1));
	if (!cg_run(child, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_test_proc_killed(child))
		goto cleanup;

	if (cg_read_key_long(child, "memory.events", "oom_kill ") <= 0)
		goto cleanup;

	parent_oom_events = cg_read_key_long(
			parent, "memory.events", "oom_kill ");
	 
	if (has_localevents && parent_oom_events != 0)
		goto cleanup;
	else if (!has_localevents && parent_oom_events <= 0)
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	if (child)
		cg_destroy(child);
	if (parent)
		cg_destroy(parent);
	free(child);
	free(parent);

	return ret;
}

 
static int test_memcg_oom_group_parent_events(const char *root)
{
	int ret = KSFT_FAIL;
	char *parent, *child;

	parent = cg_name(root, "memcg_test_0");
	child = cg_name(root, "memcg_test_0/memcg_test_1");

	if (!parent || !child)
		goto cleanup;

	if (cg_create(parent))
		goto cleanup;

	if (cg_create(child))
		goto cleanup;

	if (cg_write(parent, "memory.max", "80M"))
		goto cleanup;

	if (cg_write(parent, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(parent, "memory.oom.group", "1"))
		goto cleanup;

	cg_run_nowait(parent, alloc_anon_noexit, (void *) MB(60));
	cg_run_nowait(child, alloc_anon_noexit, (void *) MB(1));
	cg_run_nowait(child, alloc_anon_noexit, (void *) MB(1));

	if (!cg_run(child, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_test_proc_killed(child))
		goto cleanup;
	if (cg_test_proc_killed(parent))
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	if (child)
		cg_destroy(child);
	if (parent)
		cg_destroy(parent);
	free(child);
	free(parent);

	return ret;
}

 
static int test_memcg_oom_group_score_events(const char *root)
{
	int ret = KSFT_FAIL;
	char *memcg;
	int safe_pid;

	memcg = cg_name(root, "memcg_test_0");

	if (!memcg)
		goto cleanup;

	if (cg_create(memcg))
		goto cleanup;

	if (cg_write(memcg, "memory.max", "50M"))
		goto cleanup;

	if (cg_write(memcg, "memory.swap.max", "0"))
		goto cleanup;

	if (cg_write(memcg, "memory.oom.group", "1"))
		goto cleanup;

	safe_pid = cg_run_nowait(memcg, alloc_anon_noexit, (void *) MB(1));
	if (set_oom_adj_score(safe_pid, OOM_SCORE_ADJ_MIN))
		goto cleanup;

	cg_run_nowait(memcg, alloc_anon_noexit, (void *) MB(1));
	if (!cg_run(memcg, alloc_anon, (void *)MB(100)))
		goto cleanup;

	if (cg_read_key_long(memcg, "memory.events", "oom_kill ") != 3)
		goto cleanup;

	if (kill(safe_pid, SIGKILL))
		goto cleanup;

	ret = KSFT_PASS;

cleanup:
	if (memcg)
		cg_destroy(memcg);
	free(memcg);

	return ret;
}

#define T(x) { x, #x }
struct memcg_test {
	int (*fn)(const char *root);
	const char *name;
} tests[] = {
	T(test_memcg_subtree_control),
	T(test_memcg_current),
	T(test_memcg_min),
	T(test_memcg_low),
	T(test_memcg_high),
	T(test_memcg_high_sync),
	T(test_memcg_max),
	T(test_memcg_reclaim),
	T(test_memcg_oom_events),
	T(test_memcg_swap_max),
	T(test_memcg_sock),
	T(test_memcg_oom_group_leaf_events),
	T(test_memcg_oom_group_parent_events),
	T(test_memcg_oom_group_score_events),
};
#undef T

int main(int argc, char **argv)
{
	char root[PATH_MAX];
	int i, proc_status, ret = EXIT_SUCCESS;

	if (cg_find_unified_root(root, sizeof(root)))
		ksft_exit_skip("cgroup v2 isn't mounted\n");

	 
	if (cg_read_strstr(root, "cgroup.controllers", "memory"))
		ksft_exit_skip("memory controller isn't available\n");

	if (cg_read_strstr(root, "cgroup.subtree_control", "memory"))
		if (cg_write(root, "cgroup.subtree_control", "+memory"))
			ksft_exit_skip("Failed to set memory controller\n");

	proc_status = proc_mount_contains("memory_recursiveprot");
	if (proc_status < 0)
		ksft_exit_skip("Failed to query cgroup mount option\n");
	has_recursiveprot = proc_status;

	proc_status = proc_mount_contains("memory_localevents");
	if (proc_status < 0)
		ksft_exit_skip("Failed to query cgroup mount option\n");
	has_localevents = proc_status;

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		switch (tests[i].fn(root)) {
		case KSFT_PASS:
			ksft_test_result_pass("%s\n", tests[i].name);
			break;
		case KSFT_SKIP:
			ksft_test_result_skip("%s\n", tests[i].name);
			break;
		default:
			ret = EXIT_FAILURE;
			ksft_test_result_fail("%s\n", tests[i].name);
			break;
		}
	}

	return ret;
}
