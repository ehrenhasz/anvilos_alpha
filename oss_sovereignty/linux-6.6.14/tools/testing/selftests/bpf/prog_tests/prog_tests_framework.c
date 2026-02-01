

#include "test_progs.h"
#include "testing_helpers.h"

static void clear_test_state(struct test_state *state)
{
	state->error_cnt = 0;
	state->sub_succ_cnt = 0;
	state->skip_cnt = 0;
}

void test_prog_tests_framework(void)
{
	struct test_state *state = env.test_state;

	 

	 
	if (test__start_subtest("test_good_subtest"))
		test__end_subtest();
	if (!ASSERT_EQ(state->skip_cnt, 0, "skip_cnt_check"))
		return;
	if (!ASSERT_EQ(state->error_cnt, 0, "error_cnt_check"))
		return;
	if (!ASSERT_EQ(state->subtest_num, 1, "subtest_num_check"))
		return;
	clear_test_state(state);

	if (test__start_subtest("test_skip_subtest")) {
		test__skip();
		test__end_subtest();
	}
	if (test__start_subtest("test_skip_subtest")) {
		test__skip();
		test__end_subtest();
	}
	if (!ASSERT_EQ(state->skip_cnt, 2, "skip_cnt_check"))
		return;
	if (!ASSERT_EQ(state->subtest_num, 3, "subtest_num_check"))
		return;
	clear_test_state(state);

	if (test__start_subtest("test_fail_subtest")) {
		test__fail();
		test__end_subtest();
	}
	if (!ASSERT_EQ(state->error_cnt, 1, "error_cnt_check"))
		return;
	if (!ASSERT_EQ(state->subtest_num, 4, "subtest_num_check"))
		return;
	clear_test_state(state);
}
