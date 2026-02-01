
#include <test_progs.h>
#include "test_probe_read_user_str.skel.h"

static const char str1[] = "mestring";
static const char str2[] = "mestringalittlebigger";
static const char str3[] = "mestringblubblubblubblubblub";

static int test_one_str(struct test_probe_read_user_str *skel, const char *str,
			size_t len)
{
	int err, duration = 0;
	char buf[256];

	 
	memset(buf, 1, sizeof(buf));
	memcpy(buf, str, len);

	 
	skel->bss->user_ptr = buf;

	 
	usleep(1);

	 
	if (CHECK(skel->bss->ret < 0, "prog_ret", "prog returned: %ld\n",
		  skel->bss->ret))
		return 1;

	 
	err = memcmp(skel->bss->buf, str, len);
	if (CHECK(err, "memcmp", "prog copied wrong string"))
		return 1;

	 
	memset(buf, 0, sizeof(buf));
	err = memcmp(skel->bss->buf + len, buf, sizeof(buf) - len);
	if (CHECK(err, "memcmp", "trailing bytes were not stripped"))
		return 1;

	return 0;
}

void test_probe_read_user_str(void)
{
	struct test_probe_read_user_str *skel;
	int err, duration = 0;

	skel = test_probe_read_user_str__open_and_load();
	if (CHECK(!skel, "test_probe_read_user_str__open_and_load",
		  "skeleton open and load failed\n"))
		return;

	 
	skel->bss->pid = getpid();

	err = test_probe_read_user_str__attach(skel);
	if (CHECK(err, "test_probe_read_user_str__attach",
		  "skeleton attach failed: %d\n", err))
		goto out;

	if (test_one_str(skel, str1, sizeof(str1)))
		goto out;
	if (test_one_str(skel, str2, sizeof(str2)))
		goto out;
	if (test_one_str(skel, str3, sizeof(str3)))
		goto out;

out:
	test_probe_read_user_str__destroy(skel);
}
