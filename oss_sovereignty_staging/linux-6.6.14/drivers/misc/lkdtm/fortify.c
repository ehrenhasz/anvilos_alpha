
 
#include "lkdtm.h"
#include <linux/string.h>
#include <linux/slab.h>

static volatile int fortify_scratch_space;

static void lkdtm_FORTIFY_STR_OBJECT(void)
{
	struct target {
		char a[10];
		int foo;
	} target[3] = {};
	 
	volatile int size = 20;

	pr_info("trying to strcmp() past the end of a struct\n");

	strncpy(target[0].a, target[1].a, size);

	 
	fortify_scratch_space = target[0].a[3];

	pr_err("FAIL: fortify did not block a strncpy() object write overflow!\n");
	pr_expected_config(CONFIG_FORTIFY_SOURCE);
}

static void lkdtm_FORTIFY_STR_MEMBER(void)
{
	struct target {
		char a[10];
		char b[10];
	} target;
	volatile int size = 20;
	char *src;

	src = kmalloc(size, GFP_KERNEL);
	strscpy(src, "over ten bytes", size);
	size = strlen(src) + 1;

	pr_info("trying to strncpy() past the end of a struct member...\n");

	 
	strncpy(target.a, src, size);

	 
	fortify_scratch_space = target.a[3];

	pr_err("FAIL: fortify did not block a strncpy() struct member write overflow!\n");
	pr_expected_config(CONFIG_FORTIFY_SOURCE);

	kfree(src);
}

static void lkdtm_FORTIFY_MEM_OBJECT(void)
{
	int before[10];
	struct target {
		char a[10];
		int foo;
	} target = {};
	int after[10];
	 
	volatile int size = 20;

	memset(before, 0, sizeof(before));
	memset(after, 0, sizeof(after));
	fortify_scratch_space = before[5];
	fortify_scratch_space = after[5];

	pr_info("trying to memcpy() past the end of a struct\n");

	pr_info("0: %zu\n", __builtin_object_size(&target, 0));
	pr_info("1: %zu\n", __builtin_object_size(&target, 1));
	pr_info("s: %d\n", size);
	memcpy(&target, &before, size);

	 
	fortify_scratch_space = target.a[3];

	pr_err("FAIL: fortify did not block a memcpy() object write overflow!\n");
	pr_expected_config(CONFIG_FORTIFY_SOURCE);
}

static void lkdtm_FORTIFY_MEM_MEMBER(void)
{
	struct target {
		char a[10];
		char b[10];
	} target;
	volatile int size = 20;
	char *src;

	src = kmalloc(size, GFP_KERNEL);
	strscpy(src, "over ten bytes", size);
	size = strlen(src) + 1;

	pr_info("trying to memcpy() past the end of a struct member...\n");

	 
	memcpy(target.a, src, size);

	 
	fortify_scratch_space = target.a[3];

	pr_err("FAIL: fortify did not block a memcpy() struct member write overflow!\n");
	pr_expected_config(CONFIG_FORTIFY_SOURCE);

	kfree(src);
}

 
static void lkdtm_FORTIFY_STRSCPY(void)
{
	char *src;
	char dst[5];

	struct {
		union {
			char big[10];
			char src[5];
		};
	} weird = { .big = "hello!" };
	char weird_dst[sizeof(weird.src) + 1];

	src = kstrdup("foobar", GFP_KERNEL);

	if (src == NULL)
		return;

	 
	if (strscpy(dst, src, 0) != -E2BIG)
		pr_warn("FAIL: strscpy() of 0 length did not return -E2BIG\n");

	 
	if (strscpy(dst, src, sizeof(dst)) != -E2BIG)
		pr_warn("FAIL: strscpy() did not return -E2BIG while src is truncated\n");

	 
	if (strncmp(dst, "foob", sizeof(dst)) != 0)
		pr_warn("FAIL: after strscpy() dst does not contain \"foob\" but \"%s\"\n",
			dst);

	 
	src[3] = '\0';

	 
	if (strscpy(dst, src, sizeof(dst)) != 3)
		pr_warn("FAIL: strscpy() did not return 3 while src was copied entirely truncated\n");

	 
	if (strncmp(dst, "foo", sizeof(dst)) != 0)
		pr_warn("FAIL: after strscpy() dst does not contain \"foo\" but \"%s\"\n",
			dst);

	 
	strscpy(weird_dst, weird.src, sizeof(weird_dst));

	if (strcmp(weird_dst, "hello") != 0)
		pr_warn("FAIL: after strscpy() weird_dst does not contain \"hello\" but \"%s\"\n",
			weird_dst);

	 
	src[3] = 'b';

	 
	strscpy(dst, src, strlen(src));

	pr_err("FAIL: strscpy() overflow not detected!\n");
	pr_expected_config(CONFIG_FORTIFY_SOURCE);

	kfree(src);
}

static struct crashtype crashtypes[] = {
	CRASHTYPE(FORTIFY_STR_OBJECT),
	CRASHTYPE(FORTIFY_STR_MEMBER),
	CRASHTYPE(FORTIFY_MEM_OBJECT),
	CRASHTYPE(FORTIFY_MEM_MEMBER),
	CRASHTYPE(FORTIFY_STRSCPY),
};

struct crashtype_category fortify_crashtypes = {
	.crashtypes = crashtypes,
	.len	    = ARRAY_SIZE(crashtypes),
};
