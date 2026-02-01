
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <kunit/test.h>
#include <linux/string.h>

static volatile int unconst;

static void strcat_test(struct kunit *test)
{
	char dest[8];

	 
	memset(dest, 0, sizeof(dest));
	KUNIT_EXPECT_EQ(test, strlen(dest), 0);
	 
	KUNIT_EXPECT_TRUE(test, strcat(dest, "") == dest);
	KUNIT_EXPECT_STREQ(test, dest, "");
	 
	KUNIT_EXPECT_TRUE(test, strcat(dest, "four\000123") == dest);
	KUNIT_EXPECT_STREQ(test, dest, "four");
	KUNIT_EXPECT_EQ(test, dest[5], '\0');
	 
	KUNIT_EXPECT_TRUE(test, strcat(dest, "AB") == dest);
	KUNIT_EXPECT_STREQ(test, dest, "fourAB");
}

static void strncat_test(struct kunit *test)
{
	char dest[8];

	 
	memset(dest, 0, sizeof(dest));
	KUNIT_EXPECT_EQ(test, strlen(dest), 0);
	 
	KUNIT_EXPECT_TRUE(test, strncat(dest, "", 0 + unconst) == dest);
	KUNIT_EXPECT_STREQ(test, dest, "");
	 
	KUNIT_EXPECT_TRUE(test, strncat(dest, "", 1 + unconst) == dest);
	KUNIT_EXPECT_STREQ(test, dest, "");
	 
	KUNIT_EXPECT_TRUE(test, strncat(dest, "asdf", 0 + unconst) == dest);
	KUNIT_EXPECT_STREQ(test, dest, "");

	 
	KUNIT_EXPECT_TRUE(test, strncat(dest, "four\000123", 8 + unconst) == dest);
	KUNIT_EXPECT_STREQ(test, dest, "four");
	KUNIT_EXPECT_EQ(test, dest[5], '\0');
	KUNIT_EXPECT_EQ(test, dest[6], '\0');
	 
	KUNIT_EXPECT_TRUE(test, strncat(dest, "ABCD", 2 + unconst) == dest);
	KUNIT_EXPECT_STREQ(test, dest, "fourAB");
}

static void strlcat_test(struct kunit *test)
{
	char dest[8] = "";
	int len = sizeof(dest) + unconst;

	 
	KUNIT_EXPECT_EQ(test, strlen(dest), 0);
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "", len), 0);
	KUNIT_EXPECT_STREQ(test, dest, "");
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "four", 1 + unconst), 4);
	KUNIT_EXPECT_STREQ(test, dest, "");

	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "four", len), 4);
	KUNIT_EXPECT_STREQ(test, dest, "four");
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "AB", len), 6);
	KUNIT_EXPECT_STREQ(test, dest, "fourAB");
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "CD", 7 + unconst), 8);
	KUNIT_EXPECT_STREQ(test, dest, "fourAB");
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "EFG", len), 9);
	KUNIT_EXPECT_STREQ(test, dest, "fourABE");
	 
	KUNIT_EXPECT_EQ(test, strlcat(dest, "1234", len), 11);
	KUNIT_EXPECT_STREQ(test, dest, "fourABE");
}

static struct kunit_case strcat_test_cases[] = {
	KUNIT_CASE(strcat_test),
	KUNIT_CASE(strncat_test),
	KUNIT_CASE(strlcat_test),
	{}
};

static struct kunit_suite strcat_test_suite = {
	.name = "strcat",
	.test_cases = strcat_test_cases,
};

kunit_test_suite(strcat_test_suite);

MODULE_LICENSE("GPL");
