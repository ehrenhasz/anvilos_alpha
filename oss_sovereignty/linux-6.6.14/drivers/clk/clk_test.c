
 
#include <linux/clk.h>
#include <linux/clk-provider.h>

 
#include "clk.h"

#include <kunit/test.h>

#define DUMMY_CLOCK_INIT_RATE	(42 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_1	(142 * 1000 * 1000)
#define DUMMY_CLOCK_RATE_2	(242 * 1000 * 1000)

struct clk_dummy_context {
	struct clk_hw hw;
	unsigned long rate;
};

static unsigned long clk_dummy_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct clk_dummy_context *ctx =
		container_of(hw, struct clk_dummy_context, hw);

	return ctx->rate;
}

static int clk_dummy_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	 
	return 0;
}

static int clk_dummy_maximize_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	 
	if (req->max_rate < ULONG_MAX)
		req->rate = req->max_rate;

	return 0;
}

static int clk_dummy_minimize_rate(struct clk_hw *hw,
				   struct clk_rate_request *req)
{
	 
	if (req->min_rate > 0)
		req->rate = req->min_rate;

	return 0;
}

static int clk_dummy_set_rate(struct clk_hw *hw,
			      unsigned long rate,
			      unsigned long parent_rate)
{
	struct clk_dummy_context *ctx =
		container_of(hw, struct clk_dummy_context, hw);

	ctx->rate = rate;
	return 0;
}

static int clk_dummy_single_set_parent(struct clk_hw *hw, u8 index)
{
	if (index >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	return 0;
}

static u8 clk_dummy_single_get_parent(struct clk_hw *hw)
{
	return 0;
}

static const struct clk_ops clk_dummy_rate_ops = {
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_determine_rate,
	.set_rate = clk_dummy_set_rate,
};

static const struct clk_ops clk_dummy_maximize_rate_ops = {
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_maximize_rate,
	.set_rate = clk_dummy_set_rate,
};

static const struct clk_ops clk_dummy_minimize_rate_ops = {
	.recalc_rate = clk_dummy_recalc_rate,
	.determine_rate = clk_dummy_minimize_rate,
	.set_rate = clk_dummy_set_rate,
};

static const struct clk_ops clk_dummy_single_parent_ops = {
	 
	.determine_rate = __clk_mux_determine_rate_closest,
	.set_parent = clk_dummy_single_set_parent,
	.get_parent = clk_dummy_single_get_parent,
};

struct clk_multiple_parent_ctx {
	struct clk_dummy_context parents_ctx[2];
	struct clk_hw hw;
	u8 current_parent;
};

static int clk_multiple_parents_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_multiple_parent_ctx *ctx =
		container_of(hw, struct clk_multiple_parent_ctx, hw);

	if (index >= clk_hw_get_num_parents(hw))
		return -EINVAL;

	ctx->current_parent = index;

	return 0;
}

static u8 clk_multiple_parents_mux_get_parent(struct clk_hw *hw)
{
	struct clk_multiple_parent_ctx *ctx =
		container_of(hw, struct clk_multiple_parent_ctx, hw);

	return ctx->current_parent;
}

static const struct clk_ops clk_multiple_parents_mux_ops = {
	.get_parent = clk_multiple_parents_mux_get_parent,
	.set_parent = clk_multiple_parents_mux_set_parent,
	.determine_rate = __clk_mux_determine_rate_closest,
};

static const struct clk_ops clk_multiple_parents_no_reparent_mux_ops = {
	.determine_rate = clk_hw_determine_rate_no_reparent,
	.get_parent = clk_multiple_parents_mux_get_parent,
	.set_parent = clk_multiple_parents_mux_set_parent,
};

static int clk_test_init_with_ops(struct kunit *test, const struct clk_ops *ops)
{
	struct clk_dummy_context *ctx;
	struct clk_init_data init = { };
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	ctx->rate = DUMMY_CLOCK_INIT_RATE;
	test->priv = ctx;

	init.name = "test_dummy_rate";
	init.ops = ops;
	ctx->hw.init = &init;

	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static int clk_test_init(struct kunit *test)
{
	return clk_test_init_with_ops(test, &clk_dummy_rate_ops);
}

static int clk_maximize_test_init(struct kunit *test)
{
	return clk_test_init_with_ops(test, &clk_dummy_maximize_rate_ops);
}

static int clk_minimize_test_init(struct kunit *test)
{
	return clk_test_init_with_ops(test, &clk_dummy_minimize_rate_ops);
}

static void clk_test_exit(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
}

 
static void clk_test_get_rate(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, ctx->rate);

	clk_put(clk);
}

 
static void clk_test_set_get_rate(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(clk);
}

 
static void clk_test_set_set_get_rate(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_test_round_set_get_rate(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long set_rate;
	long rounded_rate;

	rounded_rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_GT(test, rounded_rate, 0);
	KUNIT_EXPECT_EQ(test, rounded_rate, DUMMY_CLOCK_RATE_1);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1),
			0);

	set_rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, set_rate, 0);
	KUNIT_EXPECT_EQ(test, rounded_rate, set_rate);

	clk_put(clk);
}

static struct kunit_case clk_test_cases[] = {
	KUNIT_CASE(clk_test_get_rate),
	KUNIT_CASE(clk_test_set_get_rate),
	KUNIT_CASE(clk_test_set_set_get_rate),
	KUNIT_CASE(clk_test_round_set_get_rate),
	{}
};

 
static struct kunit_suite clk_test_suite = {
	.name = "clk-test",
	.init = clk_test_init,
	.exit = clk_test_exit,
	.test_cases = clk_test_cases,
};

static int clk_uncached_test_init(struct kunit *test)
{
	struct clk_dummy_context *ctx;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->rate = DUMMY_CLOCK_INIT_RATE;
	ctx->hw.init = CLK_HW_INIT_NO_PARENT("test-clk",
					     &clk_dummy_rate_ops,
					     CLK_GET_RATE_NOCACHE);

	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

 
static void clk_test_uncached_get_rate(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_INIT_RATE);

	 
	ctx->rate = DUMMY_CLOCK_RATE_1;
	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(clk);
}

 
static void clk_test_uncached_set_range(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_test_uncached_updated_rate_set_range(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	 
	ctx->rate = DUMMY_CLOCK_RATE_1 + 1000;
	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1 + 1000);

	clk_put(clk);
}

static struct kunit_case clk_uncached_test_cases[] = {
	KUNIT_CASE(clk_test_uncached_get_rate),
	KUNIT_CASE(clk_test_uncached_set_range),
	KUNIT_CASE(clk_test_uncached_updated_rate_set_range),
	{}
};

 
static struct kunit_suite clk_uncached_test_suite = {
	.name = "clk-uncached-test",
	.init = clk_uncached_test_init,
	.exit = clk_test_exit,
	.test_cases = clk_uncached_test_cases,
};

static int
clk_multiple_parents_mux_test_init(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx;
	const char *parents[2] = { "parent-0", "parent-1"};
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->parents_ctx[0].hw.init = CLK_HW_INIT_NO_PARENT("parent-0",
							    &clk_dummy_rate_ops,
							    0);
	ctx->parents_ctx[0].rate = DUMMY_CLOCK_RATE_1;
	ret = clk_hw_register(NULL, &ctx->parents_ctx[0].hw);
	if (ret)
		return ret;

	ctx->parents_ctx[1].hw.init = CLK_HW_INIT_NO_PARENT("parent-1",
							    &clk_dummy_rate_ops,
							    0);
	ctx->parents_ctx[1].rate = DUMMY_CLOCK_RATE_2;
	ret = clk_hw_register(NULL, &ctx->parents_ctx[1].hw);
	if (ret)
		return ret;

	ctx->current_parent = 0;
	ctx->hw.init = CLK_HW_INIT_PARENTS("test-mux", parents,
					   &clk_multiple_parents_mux_ops,
					   CLK_SET_RATE_PARENT);
	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static void
clk_multiple_parents_mux_test_exit(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->parents_ctx[0].hw);
	clk_hw_unregister(&ctx->parents_ctx[1].hw);
}

 
static void
clk_test_multiple_parents_mux_get_parent(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent = clk_hw_get_clk(&ctx->parents_ctx[0].hw, NULL);

	KUNIT_EXPECT_TRUE(test, clk_is_match(clk_get_parent(clk), parent));

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_multiple_parents_mux_has_parent(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;

	parent = clk_hw_get_clk(&ctx->parents_ctx[0].hw, NULL);
	KUNIT_EXPECT_TRUE(test, clk_has_parent(clk, parent));
	clk_put(parent);

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_EXPECT_TRUE(test, clk_has_parent(clk, parent));
	clk_put(parent);

	clk_put(clk);
}

 
static void
clk_test_multiple_parents_mux_set_range_set_parent_get_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent1, *parent2;
	unsigned long rate;
	int ret;

	kunit_skip(test, "This needs to be fixed in the core.");

	parent1 = clk_hw_get_clk(&ctx->parents_ctx[0].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent1);
	KUNIT_ASSERT_TRUE(test, clk_is_match(clk_get_parent(clk), parent1));

	parent2 = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent2);

	ret = clk_set_rate(parent1, DUMMY_CLOCK_RATE_1);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate(parent2, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk,
				 DUMMY_CLOCK_RATE_1 - 1000,
				 DUMMY_CLOCK_RATE_1 + 1000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_parent(clk, parent2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_1 + 1000);

	clk_put(parent2);
	clk_put(parent1);
	clk_put(clk);
}

static struct kunit_case clk_multiple_parents_mux_test_cases[] = {
	KUNIT_CASE(clk_test_multiple_parents_mux_get_parent),
	KUNIT_CASE(clk_test_multiple_parents_mux_has_parent),
	KUNIT_CASE(clk_test_multiple_parents_mux_set_range_set_parent_get_rate),
	{}
};

 
static struct kunit_suite
clk_multiple_parents_mux_test_suite = {
	.name = "clk-multiple-parents-mux-test",
	.init = clk_multiple_parents_mux_test_init,
	.exit = clk_multiple_parents_mux_test_exit,
	.test_cases = clk_multiple_parents_mux_test_cases,
};

static int
clk_orphan_transparent_multiple_parent_mux_test_init(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx;
	const char *parents[2] = { "missing-parent", "proper-parent"};
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->parents_ctx[1].hw.init = CLK_HW_INIT_NO_PARENT("proper-parent",
							    &clk_dummy_rate_ops,
							    0);
	ctx->parents_ctx[1].rate = DUMMY_CLOCK_INIT_RATE;
	ret = clk_hw_register(NULL, &ctx->parents_ctx[1].hw);
	if (ret)
		return ret;

	ctx->hw.init = CLK_HW_INIT_PARENTS("test-orphan-mux", parents,
					   &clk_multiple_parents_mux_ops,
					   CLK_SET_RATE_PARENT);
	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static void
clk_orphan_transparent_multiple_parent_mux_test_exit(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->parents_ctx[1].hw);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_get_parent(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);

	KUNIT_EXPECT_PTR_EQ(test, clk_get_parent(clk), NULL);

	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent, *new_parent;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	new_parent = clk_get_parent(clk);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);
	KUNIT_EXPECT_TRUE(test, clk_is_match(parent, new_parent));

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent_drop_range(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	unsigned long parent_rate, new_parent_rate;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_drop_range(clk);
	KUNIT_ASSERT_EQ(test, ret, 0);

	new_parent_rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, new_parent_rate, 0);
	KUNIT_EXPECT_EQ(test, parent_rate, new_parent_rate);

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent_get_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	unsigned long parent_rate, rate;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, parent_rate, rate);

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent_put(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk *clk, *parent;
	unsigned long parent_rate, new_parent_rate;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	clk = clk_hw_get_clk(&ctx->hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, clk);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	clk_put(clk);

	new_parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, new_parent_rate, 0);
	KUNIT_EXPECT_EQ(test, parent_rate, new_parent_rate);

	clk_put(parent);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent_set_range_modified(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	unsigned long rate;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_parent_set_range_untouched(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	unsigned long parent_rate, new_parent_rate;
	int ret;

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk,
				 DUMMY_CLOCK_INIT_RATE - 1000,
				 DUMMY_CLOCK_INIT_RATE + 1000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	new_parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, new_parent_rate, 0);
	KUNIT_EXPECT_EQ(test, parent_rate, new_parent_rate);

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_range_round_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	long rate;
	int ret;

	ret = clk_set_rate_range(clk, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void
clk_test_orphan_transparent_multiple_parent_mux_set_range_set_parent_get_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	unsigned long rate;
	int ret;

	kunit_skip(test, "This needs to be fixed in the core.");

	clk_hw_set_rate_range(hw, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);

	parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, parent);

	ret = clk_set_parent(clk, parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(parent);
	clk_put(clk);
}

static struct kunit_case clk_orphan_transparent_multiple_parent_mux_test_cases[] = {
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_get_parent),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent_drop_range),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent_get_rate),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent_put),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent_set_range_modified),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_parent_set_range_untouched),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_range_round_rate),
	KUNIT_CASE(clk_test_orphan_transparent_multiple_parent_mux_set_range_set_parent_get_rate),
	{}
};

 
static struct kunit_suite clk_orphan_transparent_multiple_parent_mux_test_suite = {
	.name = "clk-orphan-transparent-multiple-parent-mux-test",
	.init = clk_orphan_transparent_multiple_parent_mux_test_init,
	.exit = clk_orphan_transparent_multiple_parent_mux_test_exit,
	.test_cases = clk_orphan_transparent_multiple_parent_mux_test_cases,
};

struct clk_single_parent_ctx {
	struct clk_dummy_context parent_ctx;
	struct clk_hw hw;
};

static int clk_single_parent_mux_test_init(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->parent_ctx.rate = DUMMY_CLOCK_INIT_RATE;
	ctx->parent_ctx.hw.init =
		CLK_HW_INIT_NO_PARENT("parent-clk",
				      &clk_dummy_rate_ops,
				      0);

	ret = clk_hw_register(NULL, &ctx->parent_ctx.hw);
	if (ret)
		return ret;

	ctx->hw.init = CLK_HW_INIT("test-clk", "parent-clk",
				   &clk_dummy_single_parent_ops,
				   CLK_SET_RATE_PARENT);

	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static void
clk_single_parent_mux_test_exit(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->parent_ctx.hw);
}

 
static void
clk_test_single_parent_mux_get_parent(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent = clk_hw_get_clk(&ctx->parent_ctx.hw, NULL);

	KUNIT_EXPECT_TRUE(test, clk_is_match(clk_get_parent(clk), parent));

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_has_parent(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent = clk_hw_get_clk(&ctx->parent_ctx.hw, NULL);

	KUNIT_EXPECT_TRUE(test, clk_has_parent(clk, parent));

	clk_put(parent);
	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_set_range_disjoint_child_last(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	int ret;

	kunit_skip(test, "This needs to be fixed in the core.");

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	ret = clk_set_rate_range(parent, 1000, 2000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk, 3000, 4000);
	KUNIT_EXPECT_LT(test, ret, 0);

	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_set_range_disjoint_parent_last(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	int ret;

	kunit_skip(test, "This needs to be fixed in the core.");

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	ret = clk_set_rate_range(clk, 1000, 2000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(parent, 3000, 4000);
	KUNIT_EXPECT_LT(test, ret, 0);

	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_set_range_round_rate_parent_only(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	long rate;
	int ret;

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	ret = clk_set_rate_range(parent, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_set_range_round_rate_child_smaller(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	long rate;
	int ret;

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	ret = clk_set_rate_range(parent, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk, DUMMY_CLOCK_RATE_1 + 1000, DUMMY_CLOCK_RATE_2 - 1000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1 + 1000);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2 - 1000);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_2 + 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1 + 1000);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2 - 1000);

	clk_put(clk);
}

 
static void
clk_test_single_parent_mux_set_range_round_rate_parent_smaller(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *parent;
	long rate;
	int ret;

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	ret = clk_set_rate_range(parent, DUMMY_CLOCK_RATE_1 + 1000, DUMMY_CLOCK_RATE_2 - 1000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = clk_set_rate_range(clk, DUMMY_CLOCK_RATE_1, DUMMY_CLOCK_RATE_2);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1 + 1000);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2 - 1000);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_2 + 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1 + 1000);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2 - 1000);

	clk_put(clk);
}

static struct kunit_case clk_single_parent_mux_test_cases[] = {
	KUNIT_CASE(clk_test_single_parent_mux_get_parent),
	KUNIT_CASE(clk_test_single_parent_mux_has_parent),
	KUNIT_CASE(clk_test_single_parent_mux_set_range_disjoint_child_last),
	KUNIT_CASE(clk_test_single_parent_mux_set_range_disjoint_parent_last),
	KUNIT_CASE(clk_test_single_parent_mux_set_range_round_rate_child_smaller),
	KUNIT_CASE(clk_test_single_parent_mux_set_range_round_rate_parent_only),
	KUNIT_CASE(clk_test_single_parent_mux_set_range_round_rate_parent_smaller),
	{}
};

 
static struct kunit_suite
clk_single_parent_mux_test_suite = {
	.name = "clk-single-parent-mux-test",
	.init = clk_single_parent_mux_test_init,
	.exit = clk_single_parent_mux_test_exit,
	.test_cases = clk_single_parent_mux_test_cases,
};

static int clk_orphan_transparent_single_parent_mux_test_init(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx;
	struct clk_init_data init = { };
	const char * const parents[] = { "orphan_parent" };
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	init.name = "test_orphan_dummy_parent";
	init.ops = &clk_dummy_single_parent_ops;
	init.parent_names = parents;
	init.num_parents = ARRAY_SIZE(parents);
	init.flags = CLK_SET_RATE_PARENT;
	ctx->hw.init = &init;

	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	memset(&init, 0, sizeof(init));
	init.name = "orphan_parent";
	init.ops = &clk_dummy_rate_ops;
	ctx->parent_ctx.hw.init = &init;
	ctx->parent_ctx.rate = DUMMY_CLOCK_INIT_RATE;

	ret = clk_hw_register(NULL, &ctx->parent_ctx.hw);
	if (ret)
		return ret;

	return 0;
}

 
static void clk_test_orphan_transparent_parent_mux_set_range(struct kunit *test)
{
	struct clk_single_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate, new_rate;

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   ctx->parent_ctx.rate - 1000,
					   ctx->parent_ctx.rate + 1000),
			0);

	new_rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, new_rate, 0);
	KUNIT_EXPECT_EQ(test, rate, new_rate);

	clk_put(clk);
}

static struct kunit_case clk_orphan_transparent_single_parent_mux_test_cases[] = {
	KUNIT_CASE(clk_test_orphan_transparent_parent_mux_set_range),
	{}
};

 
static struct kunit_suite clk_orphan_transparent_single_parent_test_suite = {
	.name = "clk-orphan-transparent-single-parent-test",
	.init = clk_orphan_transparent_single_parent_mux_test_init,
	.exit = clk_single_parent_mux_test_exit,
	.test_cases = clk_orphan_transparent_single_parent_mux_test_cases,
};

struct clk_single_parent_two_lvl_ctx {
	struct clk_dummy_context parent_parent_ctx;
	struct clk_dummy_context parent_ctx;
	struct clk_hw hw;
};

static int
clk_orphan_two_level_root_last_test_init(struct kunit *test)
{
	struct clk_single_parent_two_lvl_ctx *ctx;
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->parent_ctx.hw.init =
		CLK_HW_INIT("intermediate-parent",
			    "root-parent",
			    &clk_dummy_single_parent_ops,
			    CLK_SET_RATE_PARENT);
	ret = clk_hw_register(NULL, &ctx->parent_ctx.hw);
	if (ret)
		return ret;

	ctx->hw.init =
		CLK_HW_INIT("test-clk", "intermediate-parent",
			    &clk_dummy_single_parent_ops,
			    CLK_SET_RATE_PARENT);
	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	ctx->parent_parent_ctx.rate = DUMMY_CLOCK_INIT_RATE;
	ctx->parent_parent_ctx.hw.init =
		CLK_HW_INIT_NO_PARENT("root-parent",
				      &clk_dummy_rate_ops,
				      0);
	ret = clk_hw_register(NULL, &ctx->parent_parent_ctx.hw);
	if (ret)
		return ret;

	return 0;
}

static void
clk_orphan_two_level_root_last_test_exit(struct kunit *test)
{
	struct clk_single_parent_two_lvl_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->parent_ctx.hw);
	clk_hw_unregister(&ctx->parent_parent_ctx.hw);
}

 
static void
clk_orphan_two_level_root_last_test_get_rate(struct kunit *test)
{
	struct clk_single_parent_two_lvl_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	rate = clk_get_rate(clk);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_INIT_RATE);

	clk_put(clk);
}

 
static void
clk_orphan_two_level_root_last_test_set_range(struct kunit *test)
{
	struct clk_single_parent_two_lvl_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;
	int ret;

	ret = clk_set_rate_range(clk,
				 DUMMY_CLOCK_INIT_RATE - 1000,
				 DUMMY_CLOCK_INIT_RATE + 1000);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_INIT_RATE);

	clk_put(clk);
}

static struct kunit_case
clk_orphan_two_level_root_last_test_cases[] = {
	KUNIT_CASE(clk_orphan_two_level_root_last_test_get_rate),
	KUNIT_CASE(clk_orphan_two_level_root_last_test_set_range),
	{}
};

 
static struct kunit_suite
clk_orphan_two_level_root_last_test_suite = {
	.name = "clk-orphan-two-level-root-last-test",
	.init = clk_orphan_two_level_root_last_test_init,
	.exit = clk_orphan_two_level_root_last_test_exit,
	.test_cases = clk_orphan_two_level_root_last_test_cases,
};

 
static void clk_range_test_set_range(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_set_range_invalid(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);

	KUNIT_EXPECT_LT(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1 + 1000,
					   DUMMY_CLOCK_RATE_1),
			0);

	clk_put(clk);
}

 
static void clk_range_test_multiple_disjoints_range(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *user1, *user2;

	user1 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user1);

	user2 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user1, 1000, 2000),
			0);

	KUNIT_EXPECT_LT(test,
			clk_set_rate_range(user2, 3000, 4000),
			0);

	clk_put(user2);
	clk_put(user1);
}

 
static void clk_range_test_set_range_round_rate_lower(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_set_range_set_rate_lower(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1 - 1000),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_set_range_set_round_rate_consistent_lower(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	long rounded;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rounded = clk_round_rate(clk, DUMMY_CLOCK_RATE_1 - 1000);
	KUNIT_ASSERT_GT(test, rounded, 0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1 - 1000),
			0);

	KUNIT_EXPECT_EQ(test, rounded, clk_get_rate(clk));

	clk_put(clk);
}

 
static void clk_range_test_set_range_round_rate_higher(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_round_rate(clk, DUMMY_CLOCK_RATE_2 + 1000);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_set_range_set_rate_higher(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_GE(test, rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_LE(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_set_range_set_round_rate_consistent_higher(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	long rounded;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rounded = clk_round_rate(clk, DUMMY_CLOCK_RATE_2 + 1000);
	KUNIT_ASSERT_GT(test, rounded, 0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	KUNIT_EXPECT_EQ(test, rounded, clk_get_rate(clk));

	clk_put(clk);
}

 
static void clk_range_test_set_range_get_rate_raised(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1 - 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(clk);
}

 
static void clk_range_test_set_range_get_rate_lowered(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

static struct kunit_case clk_range_test_cases[] = {
	KUNIT_CASE(clk_range_test_set_range),
	KUNIT_CASE(clk_range_test_set_range_invalid),
	KUNIT_CASE(clk_range_test_multiple_disjoints_range),
	KUNIT_CASE(clk_range_test_set_range_round_rate_lower),
	KUNIT_CASE(clk_range_test_set_range_set_rate_lower),
	KUNIT_CASE(clk_range_test_set_range_set_round_rate_consistent_lower),
	KUNIT_CASE(clk_range_test_set_range_round_rate_higher),
	KUNIT_CASE(clk_range_test_set_range_set_rate_higher),
	KUNIT_CASE(clk_range_test_set_range_set_round_rate_consistent_higher),
	KUNIT_CASE(clk_range_test_set_range_get_rate_raised),
	KUNIT_CASE(clk_range_test_set_range_get_rate_lowered),
	{}
};

 
static struct kunit_suite clk_range_test_suite = {
	.name = "clk-range-test",
	.init = clk_test_init,
	.exit = clk_test_exit,
	.test_cases = clk_range_test_cases,
};

 
static void clk_range_test_set_range_rate_maximized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2 - 1000),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2 - 1000);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(clk);
}

 
static void clk_range_test_multiple_set_range_rate_maximized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *user1, *user2;
	unsigned long rate;

	user1 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user1);

	user2 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user1,
					   0,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user2,
					   0,
					   DUMMY_CLOCK_RATE_1),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	KUNIT_ASSERT_EQ(test,
			clk_drop_range(user2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(user2);
	clk_put(user1);
	clk_put(clk);
}

 
static void clk_range_test_multiple_set_range_rate_put_maximized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *user1, *user2;
	unsigned long rate;

	user1 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user1);

	user2 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_2 + 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user1,
					   0,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user2,
					   0,
					   DUMMY_CLOCK_RATE_1),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(user2);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(user1);
	clk_put(clk);
}

static struct kunit_case clk_range_maximize_test_cases[] = {
	KUNIT_CASE(clk_range_test_set_range_rate_maximized),
	KUNIT_CASE(clk_range_test_multiple_set_range_rate_maximized),
	KUNIT_CASE(clk_range_test_multiple_set_range_rate_put_maximized),
	{}
};

 
static struct kunit_suite clk_range_maximize_test_suite = {
	.name = "clk-range-maximize-test",
	.init = clk_maximize_test_init,
	.exit = clk_test_exit,
	.test_cases = clk_range_maximize_test_cases,
};

 
static void clk_range_test_set_range_rate_minimized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	unsigned long rate;

	KUNIT_ASSERT_EQ(test,
			clk_set_rate(clk, DUMMY_CLOCK_RATE_1 - 1000),
			0);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1 + 1000,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1 + 1000);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(clk,
					   DUMMY_CLOCK_RATE_1,
					   DUMMY_CLOCK_RATE_2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(clk);
}

 
static void clk_range_test_multiple_set_range_rate_minimized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *user1, *user2;
	unsigned long rate;

	user1 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user1);

	user2 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user1,
					   DUMMY_CLOCK_RATE_1,
					   ULONG_MAX),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user2,
					   DUMMY_CLOCK_RATE_2,
					   ULONG_MAX),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	KUNIT_ASSERT_EQ(test,
			clk_drop_range(user2),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(user2);
	clk_put(user1);
	clk_put(clk);
}

 
static void clk_range_test_multiple_set_range_rate_put_minimized(struct kunit *test)
{
	struct clk_dummy_context *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *user1, *user2;
	unsigned long rate;

	user1 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user1);

	user2 = clk_hw_get_clk(hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, user2);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user1,
					   DUMMY_CLOCK_RATE_1,
					   ULONG_MAX),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	KUNIT_ASSERT_EQ(test,
			clk_set_rate_range(user2,
					   DUMMY_CLOCK_RATE_2,
					   ULONG_MAX),
			0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_2);

	clk_put(user2);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_put(user1);
	clk_put(clk);
}

static struct kunit_case clk_range_minimize_test_cases[] = {
	KUNIT_CASE(clk_range_test_set_range_rate_minimized),
	KUNIT_CASE(clk_range_test_multiple_set_range_rate_minimized),
	KUNIT_CASE(clk_range_test_multiple_set_range_rate_put_minimized),
	{}
};

 
static struct kunit_suite clk_range_minimize_test_suite = {
	.name = "clk-range-minimize-test",
	.init = clk_minimize_test_init,
	.exit = clk_test_exit,
	.test_cases = clk_range_minimize_test_cases,
};

struct clk_leaf_mux_ctx {
	struct clk_multiple_parent_ctx mux_ctx;
	struct clk_hw hw;
};

static int
clk_leaf_mux_set_rate_parent_test_init(struct kunit *test)
{
	struct clk_leaf_mux_ctx *ctx;
	const char *top_parents[2] = { "parent-0", "parent-1" };
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->mux_ctx.parents_ctx[0].hw.init = CLK_HW_INIT_NO_PARENT("parent-0",
								    &clk_dummy_rate_ops,
								    0);
	ctx->mux_ctx.parents_ctx[0].rate = DUMMY_CLOCK_RATE_1;
	ret = clk_hw_register(NULL, &ctx->mux_ctx.parents_ctx[0].hw);
	if (ret)
		return ret;

	ctx->mux_ctx.parents_ctx[1].hw.init = CLK_HW_INIT_NO_PARENT("parent-1",
								    &clk_dummy_rate_ops,
								    0);
	ctx->mux_ctx.parents_ctx[1].rate = DUMMY_CLOCK_RATE_2;
	ret = clk_hw_register(NULL, &ctx->mux_ctx.parents_ctx[1].hw);
	if (ret)
		return ret;

	ctx->mux_ctx.current_parent = 0;
	ctx->mux_ctx.hw.init = CLK_HW_INIT_PARENTS("test-mux", top_parents,
						   &clk_multiple_parents_mux_ops,
						   0);
	ret = clk_hw_register(NULL, &ctx->mux_ctx.hw);
	if (ret)
		return ret;

	ctx->hw.init = CLK_HW_INIT_HW("test-clock", &ctx->mux_ctx.hw,
				      &clk_dummy_single_parent_ops,
				      CLK_SET_RATE_PARENT);
	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static void clk_leaf_mux_set_rate_parent_test_exit(struct kunit *test)
{
	struct clk_leaf_mux_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->mux_ctx.hw);
	clk_hw_unregister(&ctx->mux_ctx.parents_ctx[0].hw);
	clk_hw_unregister(&ctx->mux_ctx.parents_ctx[1].hw);
}

 
static void clk_leaf_mux_set_rate_parent_determine_rate(struct kunit *test)
{
	struct clk_leaf_mux_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk_rate_request req;
	unsigned long rate;
	int ret;

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_EQ(test, rate, DUMMY_CLOCK_RATE_1);

	clk_hw_init_rate_request(hw, &req, DUMMY_CLOCK_RATE_2);

	ret = __clk_determine_rate(hw, &req);
	KUNIT_ASSERT_EQ(test, ret, 0);

	KUNIT_EXPECT_EQ(test, req.rate, DUMMY_CLOCK_RATE_2);
	KUNIT_EXPECT_EQ(test, req.best_parent_rate, DUMMY_CLOCK_RATE_2);
	KUNIT_EXPECT_PTR_EQ(test, req.best_parent_hw, &ctx->mux_ctx.hw);

	clk_put(clk);
}

static struct kunit_case clk_leaf_mux_set_rate_parent_test_cases[] = {
	KUNIT_CASE(clk_leaf_mux_set_rate_parent_determine_rate),
	{}
};

 
static struct kunit_suite clk_leaf_mux_set_rate_parent_test_suite = {
	.name = "clk-leaf-mux-set-rate-parent",
	.init = clk_leaf_mux_set_rate_parent_test_init,
	.exit = clk_leaf_mux_set_rate_parent_test_exit,
	.test_cases = clk_leaf_mux_set_rate_parent_test_cases,
};

struct clk_mux_notifier_rate_change {
	bool done;
	unsigned long old_rate;
	unsigned long new_rate;
	wait_queue_head_t wq;
};

struct clk_mux_notifier_ctx {
	struct clk_multiple_parent_ctx mux_ctx;
	struct clk *clk;
	struct notifier_block clk_nb;
	struct clk_mux_notifier_rate_change pre_rate_change;
	struct clk_mux_notifier_rate_change post_rate_change;
};

#define NOTIFIER_TIMEOUT_MS 100

static int clk_mux_notifier_callback(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct clk_notifier_data *clk_data = data;
	struct clk_mux_notifier_ctx *ctx = container_of(nb,
							struct clk_mux_notifier_ctx,
							clk_nb);

	if (action & PRE_RATE_CHANGE) {
		ctx->pre_rate_change.old_rate = clk_data->old_rate;
		ctx->pre_rate_change.new_rate = clk_data->new_rate;
		ctx->pre_rate_change.done = true;
		wake_up_interruptible(&ctx->pre_rate_change.wq);
	}

	if (action & POST_RATE_CHANGE) {
		ctx->post_rate_change.old_rate = clk_data->old_rate;
		ctx->post_rate_change.new_rate = clk_data->new_rate;
		ctx->post_rate_change.done = true;
		wake_up_interruptible(&ctx->post_rate_change.wq);
	}

	return 0;
}

static int clk_mux_notifier_test_init(struct kunit *test)
{
	struct clk_mux_notifier_ctx *ctx;
	const char *top_parents[2] = { "parent-0", "parent-1" };
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;
	ctx->clk_nb.notifier_call = clk_mux_notifier_callback;
	init_waitqueue_head(&ctx->pre_rate_change.wq);
	init_waitqueue_head(&ctx->post_rate_change.wq);

	ctx->mux_ctx.parents_ctx[0].hw.init = CLK_HW_INIT_NO_PARENT("parent-0",
								    &clk_dummy_rate_ops,
								    0);
	ctx->mux_ctx.parents_ctx[0].rate = DUMMY_CLOCK_RATE_1;
	ret = clk_hw_register(NULL, &ctx->mux_ctx.parents_ctx[0].hw);
	if (ret)
		return ret;

	ctx->mux_ctx.parents_ctx[1].hw.init = CLK_HW_INIT_NO_PARENT("parent-1",
								    &clk_dummy_rate_ops,
								    0);
	ctx->mux_ctx.parents_ctx[1].rate = DUMMY_CLOCK_RATE_2;
	ret = clk_hw_register(NULL, &ctx->mux_ctx.parents_ctx[1].hw);
	if (ret)
		return ret;

	ctx->mux_ctx.current_parent = 0;
	ctx->mux_ctx.hw.init = CLK_HW_INIT_PARENTS("test-mux", top_parents,
						   &clk_multiple_parents_mux_ops,
						   0);
	ret = clk_hw_register(NULL, &ctx->mux_ctx.hw);
	if (ret)
		return ret;

	ctx->clk = clk_hw_get_clk(&ctx->mux_ctx.hw, NULL);
	ret = clk_notifier_register(ctx->clk, &ctx->clk_nb);
	if (ret)
		return ret;

	return 0;
}

static void clk_mux_notifier_test_exit(struct kunit *test)
{
	struct clk_mux_notifier_ctx *ctx = test->priv;
	struct clk *clk = ctx->clk;

	clk_notifier_unregister(clk, &ctx->clk_nb);
	clk_put(clk);

	clk_hw_unregister(&ctx->mux_ctx.hw);
	clk_hw_unregister(&ctx->mux_ctx.parents_ctx[0].hw);
	clk_hw_unregister(&ctx->mux_ctx.parents_ctx[1].hw);
}

 
static void clk_mux_notifier_set_parent_test(struct kunit *test)
{
	struct clk_mux_notifier_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->mux_ctx.hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *new_parent = clk_hw_get_clk(&ctx->mux_ctx.parents_ctx[1].hw, NULL);
	int ret;

	ret = clk_set_parent(clk, new_parent);
	KUNIT_ASSERT_EQ(test, ret, 0);

	ret = wait_event_interruptible_timeout(ctx->pre_rate_change.wq,
					       ctx->pre_rate_change.done,
					       msecs_to_jiffies(NOTIFIER_TIMEOUT_MS));
	KUNIT_ASSERT_GT(test, ret, 0);

	KUNIT_EXPECT_EQ(test, ctx->pre_rate_change.old_rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_EQ(test, ctx->pre_rate_change.new_rate, DUMMY_CLOCK_RATE_2);

	ret = wait_event_interruptible_timeout(ctx->post_rate_change.wq,
					       ctx->post_rate_change.done,
					       msecs_to_jiffies(NOTIFIER_TIMEOUT_MS));
	KUNIT_ASSERT_GT(test, ret, 0);

	KUNIT_EXPECT_EQ(test, ctx->post_rate_change.old_rate, DUMMY_CLOCK_RATE_1);
	KUNIT_EXPECT_EQ(test, ctx->post_rate_change.new_rate, DUMMY_CLOCK_RATE_2);

	clk_put(new_parent);
	clk_put(clk);
}

static struct kunit_case clk_mux_notifier_test_cases[] = {
	KUNIT_CASE(clk_mux_notifier_set_parent_test),
	{}
};

 
static struct kunit_suite clk_mux_notifier_test_suite = {
	.name = "clk-mux-notifier",
	.init = clk_mux_notifier_test_init,
	.exit = clk_mux_notifier_test_exit,
	.test_cases = clk_mux_notifier_test_cases,
};

static int
clk_mux_no_reparent_test_init(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx;
	const char *parents[2] = { "parent-0", "parent-1"};
	int ret;

	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;

	ctx->parents_ctx[0].hw.init = CLK_HW_INIT_NO_PARENT("parent-0",
							    &clk_dummy_rate_ops,
							    0);
	ctx->parents_ctx[0].rate = DUMMY_CLOCK_RATE_1;
	ret = clk_hw_register(NULL, &ctx->parents_ctx[0].hw);
	if (ret)
		return ret;

	ctx->parents_ctx[1].hw.init = CLK_HW_INIT_NO_PARENT("parent-1",
							    &clk_dummy_rate_ops,
							    0);
	ctx->parents_ctx[1].rate = DUMMY_CLOCK_RATE_2;
	ret = clk_hw_register(NULL, &ctx->parents_ctx[1].hw);
	if (ret)
		return ret;

	ctx->current_parent = 0;
	ctx->hw.init = CLK_HW_INIT_PARENTS("test-mux", parents,
					   &clk_multiple_parents_no_reparent_mux_ops,
					   0);
	ret = clk_hw_register(NULL, &ctx->hw);
	if (ret)
		return ret;

	return 0;
}

static void
clk_mux_no_reparent_test_exit(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;

	clk_hw_unregister(&ctx->hw);
	clk_hw_unregister(&ctx->parents_ctx[0].hw);
	clk_hw_unregister(&ctx->parents_ctx[1].hw);
}

 
static void clk_mux_no_reparent_round_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *other_parent, *parent;
	unsigned long other_parent_rate;
	unsigned long parent_rate;
	long rounded_rate;

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	other_parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, other_parent);
	KUNIT_ASSERT_FALSE(test, clk_is_match(parent, other_parent));

	other_parent_rate = clk_get_rate(other_parent);
	KUNIT_ASSERT_GT(test, other_parent_rate, 0);
	clk_put(other_parent);

	rounded_rate = clk_round_rate(clk, other_parent_rate);
	KUNIT_ASSERT_GT(test, rounded_rate, 0);
	KUNIT_EXPECT_EQ(test, rounded_rate, parent_rate);

	clk_put(clk);
}

 
static void clk_mux_no_reparent_set_rate(struct kunit *test)
{
	struct clk_multiple_parent_ctx *ctx = test->priv;
	struct clk_hw *hw = &ctx->hw;
	struct clk *clk = clk_hw_get_clk(hw, NULL);
	struct clk *other_parent, *parent;
	unsigned long other_parent_rate;
	unsigned long parent_rate;
	unsigned long rate;
	int ret;

	parent = clk_get_parent(clk);
	KUNIT_ASSERT_PTR_NE(test, parent, NULL);

	parent_rate = clk_get_rate(parent);
	KUNIT_ASSERT_GT(test, parent_rate, 0);

	other_parent = clk_hw_get_clk(&ctx->parents_ctx[1].hw, NULL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, other_parent);
	KUNIT_ASSERT_FALSE(test, clk_is_match(parent, other_parent));

	other_parent_rate = clk_get_rate(other_parent);
	KUNIT_ASSERT_GT(test, other_parent_rate, 0);
	clk_put(other_parent);

	ret = clk_set_rate(clk, other_parent_rate);
	KUNIT_ASSERT_EQ(test, ret, 0);

	rate = clk_get_rate(clk);
	KUNIT_ASSERT_GT(test, rate, 0);
	KUNIT_EXPECT_EQ(test, rate, parent_rate);

	clk_put(clk);
}

static struct kunit_case clk_mux_no_reparent_test_cases[] = {
	KUNIT_CASE(clk_mux_no_reparent_round_rate),
	KUNIT_CASE(clk_mux_no_reparent_set_rate),
	{}
};

 
static struct kunit_suite clk_mux_no_reparent_test_suite = {
	.name = "clk-mux-no-reparent",
	.init = clk_mux_no_reparent_test_init,
	.exit = clk_mux_no_reparent_test_exit,
	.test_cases = clk_mux_no_reparent_test_cases,
};

kunit_test_suites(
	&clk_leaf_mux_set_rate_parent_test_suite,
	&clk_test_suite,
	&clk_multiple_parents_mux_test_suite,
	&clk_mux_no_reparent_test_suite,
	&clk_mux_notifier_test_suite,
	&clk_orphan_transparent_multiple_parent_mux_test_suite,
	&clk_orphan_transparent_single_parent_test_suite,
	&clk_orphan_two_level_root_last_test_suite,
	&clk_range_test_suite,
	&clk_range_maximize_test_suite,
	&clk_range_minimize_test_suite,
	&clk_single_parent_mux_test_suite,
	&clk_uncached_test_suite
);
MODULE_LICENSE("GPL v2");
