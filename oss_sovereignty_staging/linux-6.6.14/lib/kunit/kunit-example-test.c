
 

#include <kunit/test.h>
#include <kunit/static_stub.h>

 
static void example_simple_test(struct kunit *test)
{
	 
	KUNIT_EXPECT_EQ(test, 1 + 1, 2);
}

 
static int example_test_init(struct kunit *test)
{
	kunit_info(test, "initializing\n");

	return 0;
}

 
static void example_test_exit(struct kunit *test)
{
	kunit_info(test, "cleaning up\n");
}


 
static int example_test_init_suite(struct kunit_suite *suite)
{
	kunit_info(suite, "initializing suite\n");

	return 0;
}

 
static void example_test_exit_suite(struct kunit_suite *suite)
{
	kunit_info(suite, "exiting suite\n");
}


 
static void example_skip_test(struct kunit *test)
{
	 
	kunit_info(test, "You should not see a line below.");

	 
	kunit_skip(test, "this test should be skipped");

	 
	KUNIT_FAIL(test, "You should not see this line.");
}

 
static void example_mark_skipped_test(struct kunit *test)
{
	 
	kunit_info(test, "You should see a line below.");

	 
	kunit_mark_skipped(test, "this test should be skipped");

	 
	kunit_info(test, "You should see this line.");
}

 
static void example_all_expect_macros_test(struct kunit *test)
{
	const u32 array1[] = { 0x0F, 0xFF };
	const u32 array2[] = { 0x1F, 0xFF };

	 
	KUNIT_EXPECT_TRUE(test, true);
	KUNIT_EXPECT_FALSE(test, false);

	 
	KUNIT_EXPECT_EQ(test, 1, 1);  
	KUNIT_EXPECT_GE(test, 1, 1);  
	KUNIT_EXPECT_LE(test, 1, 1);  
	KUNIT_EXPECT_NE(test, 1, 0);  
	KUNIT_EXPECT_GT(test, 1, 0);  
	KUNIT_EXPECT_LT(test, 0, 1);  

	 
	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, test);
	KUNIT_EXPECT_PTR_EQ(test, NULL, NULL);
	KUNIT_EXPECT_PTR_NE(test, test, NULL);
	KUNIT_EXPECT_NULL(test, NULL);
	KUNIT_EXPECT_NOT_NULL(test, test);

	 
	KUNIT_EXPECT_STREQ(test, "hi", "hi");
	KUNIT_EXPECT_STRNEQ(test, "hi", "bye");

	 
	KUNIT_EXPECT_MEMEQ(test, array1, array1, sizeof(array1));
	KUNIT_EXPECT_MEMNEQ(test, array1, array2, sizeof(array1));

	 
	KUNIT_ASSERT_GT(test, sizeof(char), 0);

	 
	KUNIT_EXPECT_GT_MSG(test, sizeof(int), 0, "Your ints are 0-bit?!");
	KUNIT_ASSERT_GT_MSG(test, sizeof(int), 0, "Your ints are 0-bit?!");
}

 
static int add_one(int i)
{
	 
	KUNIT_STATIC_STUB_REDIRECT(add_one, i);

	return i + 1;
}

 
static int subtract_one(int i)
{
	 

	return i - 1;
}

 
static void example_static_stub_test(struct kunit *test)
{
	 
	KUNIT_EXPECT_EQ(test, add_one(1), 2);

	 
	kunit_activate_static_stub(test, add_one, subtract_one);

	 
	KUNIT_EXPECT_EQ(test, add_one(1), 0);

	 
	kunit_deactivate_static_stub(test, add_one);
	KUNIT_EXPECT_EQ(test, add_one(1), 2);
}

static const struct example_param {
	int value;
} example_params_array[] = {
	{ .value = 2, },
	{ .value = 1, },
	{ .value = 0, },
};

static void example_param_get_desc(const struct example_param *p, char *desc)
{
	snprintf(desc, KUNIT_PARAM_DESC_SIZE, "example value %d", p->value);
}

KUNIT_ARRAY_PARAM(example, example_params_array, example_param_get_desc);

 
static void example_params_test(struct kunit *test)
{
	const struct example_param *param = test->param_value;

	 
	KUNIT_ASSERT_NOT_NULL(test, param);

	 
	if (!param->value)
		kunit_skip(test, "unsupported param value");

	 
	KUNIT_EXPECT_EQ(test, param->value % param->value, 0);
}

 
static void example_slow_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 1 + 1, 2);
}

 
static struct kunit_case example_test_cases[] = {
	 
	KUNIT_CASE(example_simple_test),
	KUNIT_CASE(example_skip_test),
	KUNIT_CASE(example_mark_skipped_test),
	KUNIT_CASE(example_all_expect_macros_test),
	KUNIT_CASE(example_static_stub_test),
	KUNIT_CASE_PARAM(example_params_test, example_gen_params),
	KUNIT_CASE_SLOW(example_slow_test),
	{}
};

 
static struct kunit_suite example_test_suite = {
	.name = "example",
	.init = example_test_init,
	.exit = example_test_exit,
	.suite_init = example_test_init_suite,
	.suite_exit = example_test_exit_suite,
	.test_cases = example_test_cases,
};

 
kunit_test_suites(&example_test_suite);

MODULE_LICENSE("GPL v2");
