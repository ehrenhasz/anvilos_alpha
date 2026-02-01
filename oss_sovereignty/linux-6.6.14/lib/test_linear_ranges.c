
 
#include <kunit/test.h>

#include <linux/linear_range.h>

 

 

#define RANGE1_MIN 10
#define RANGE1_MIN_SEL 2
#define RANGE1_STEP 10

 
static const unsigned int range1_sels[] = { RANGE1_MIN_SEL, RANGE1_MIN_SEL + 1,
					    RANGE1_MIN_SEL + 2,
					    RANGE1_MIN_SEL + 3,
					    RANGE1_MIN_SEL + 4 };
 
static const unsigned int range1_vals[] = { RANGE1_MIN, RANGE1_MIN +
					    RANGE1_STEP,
					    RANGE1_MIN + RANGE1_STEP * 2,
					    RANGE1_MIN + RANGE1_STEP * 3,
					    RANGE1_MIN + RANGE1_STEP * 4 };

#define RANGE2_MIN 100
#define RANGE2_MIN_SEL 7
#define RANGE2_STEP 50

 
static const unsigned int range2_sels[] = { RANGE2_MIN_SEL, RANGE2_MIN_SEL + 1,
					    RANGE2_MIN_SEL + 2,
					    RANGE2_MIN_SEL + 3 };
 
static const unsigned int range2_vals[] = { RANGE2_MIN, RANGE2_MIN +
					    RANGE2_STEP,
					    RANGE2_MIN + RANGE2_STEP * 2,
					    RANGE2_MIN + RANGE2_STEP * 3 };

#define RANGE1_NUM_VALS (ARRAY_SIZE(range1_vals))
#define RANGE2_NUM_VALS (ARRAY_SIZE(range2_vals))
#define RANGE_NUM_VALS (RANGE1_NUM_VALS + RANGE2_NUM_VALS)

#define RANGE1_MAX_SEL (RANGE1_MIN_SEL + RANGE1_NUM_VALS - 1)
#define RANGE1_MAX_VAL (range1_vals[RANGE1_NUM_VALS - 1])

#define RANGE2_MAX_SEL (RANGE2_MIN_SEL + RANGE2_NUM_VALS - 1)
#define RANGE2_MAX_VAL (range2_vals[RANGE2_NUM_VALS - 1])

#define SMALLEST_SEL RANGE1_MIN_SEL
#define SMALLEST_VAL RANGE1_MIN

static struct linear_range testr[] = {
	LINEAR_RANGE(RANGE1_MIN, RANGE1_MIN_SEL, RANGE1_MAX_SEL, RANGE1_STEP),
	LINEAR_RANGE(RANGE2_MIN, RANGE2_MIN_SEL, RANGE2_MAX_SEL, RANGE2_STEP),
};

static void range_test_get_value(struct kunit *test)
{
	int ret, i;
	unsigned int sel, val;

	for (i = 0; i < RANGE1_NUM_VALS; i++) {
		sel = range1_sels[i];
		ret = linear_range_get_value_array(&testr[0], 2, sel, &val);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, val, range1_vals[i]);
	}
	for (i = 0; i < RANGE2_NUM_VALS; i++) {
		sel = range2_sels[i];
		ret = linear_range_get_value_array(&testr[0], 2, sel, &val);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, val, range2_vals[i]);
	}
	ret = linear_range_get_value_array(&testr[0], 2, sel + 1, &val);
	KUNIT_EXPECT_NE(test, 0, ret);
}

static void range_test_get_selector_high(struct kunit *test)
{
	int ret, i;
	unsigned int sel;
	bool found;

	for (i = 0; i < RANGE1_NUM_VALS; i++) {
		ret = linear_range_get_selector_high(&testr[0], range1_vals[i],
						     &sel, &found);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, sel, range1_sels[i]);
		KUNIT_EXPECT_TRUE(test, found);
	}

	ret = linear_range_get_selector_high(&testr[0], RANGE1_MAX_VAL + 1,
					     &sel, &found);
	KUNIT_EXPECT_LE(test, ret, 0);

	ret = linear_range_get_selector_high(&testr[0], RANGE1_MIN - 1,
					     &sel, &found);
	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_FALSE(test, found);
	KUNIT_EXPECT_EQ(test, sel, range1_sels[0]);
}

static void range_test_get_value_amount(struct kunit *test)
{
	int ret;

	ret = linear_range_values_in_range_array(&testr[0], 2);
	KUNIT_EXPECT_EQ(test, (int)RANGE_NUM_VALS, ret);
}

static void range_test_get_selector_low(struct kunit *test)
{
	int i, ret;
	unsigned int sel;
	bool found;

	for (i = 0; i < RANGE1_NUM_VALS; i++) {
		ret = linear_range_get_selector_low_array(&testr[0], 2,
							  range1_vals[i], &sel,
							  &found);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, sel, range1_sels[i]);
		KUNIT_EXPECT_TRUE(test, found);
	}
	for (i = 0; i < RANGE2_NUM_VALS; i++) {
		ret = linear_range_get_selector_low_array(&testr[0], 2,
							  range2_vals[i], &sel,
							  &found);
		KUNIT_EXPECT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ(test, sel, range2_sels[i]);
		KUNIT_EXPECT_TRUE(test, found);
	}

	 
	ret = linear_range_get_selector_low_array(&testr[0], 2,
					range2_vals[RANGE2_NUM_VALS - 1] + 1,
					&sel, &found);

	KUNIT_EXPECT_EQ(test, 0, ret);
	KUNIT_EXPECT_EQ(test, sel, range2_sels[RANGE2_NUM_VALS - 1]);
	KUNIT_EXPECT_FALSE(test, found);
}

static struct kunit_case range_test_cases[] = {
	KUNIT_CASE(range_test_get_value_amount),
	KUNIT_CASE(range_test_get_selector_high),
	KUNIT_CASE(range_test_get_selector_low),
	KUNIT_CASE(range_test_get_value),
	{},
};

static struct kunit_suite range_test_module = {
	.name = "linear-ranges-test",
	.test_cases = range_test_cases,
};

kunit_test_suites(&range_test_module);

MODULE_LICENSE("GPL");
