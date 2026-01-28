


#ifdef CONFIG_DAMON_VADDR_KUNIT_TEST

#ifndef _DAMON_VADDR_TEST_H
#define _DAMON_VADDR_TEST_H

#include <kunit/test.h>

static int __link_vmas(struct maple_tree *mt, struct vm_area_struct *vmas,
			ssize_t nr_vmas)
{
	int i, ret = -ENOMEM;
	MA_STATE(mas, mt, 0, 0);

	if (!nr_vmas)
		return 0;

	mas_lock(&mas);
	for (i = 0; i < nr_vmas; i++) {
		mas_set_range(&mas, vmas[i].vm_start, vmas[i].vm_end - 1);
		if (mas_store_gfp(&mas, &vmas[i], GFP_KERNEL))
			goto failed;
	}

	ret = 0;
failed:
	mas_unlock(&mas);
	return ret;
}


static void damon_test_three_regions_in_vmas(struct kunit *test)
{
	static struct mm_struct mm;
	struct damon_addr_range regions[3] = {0,};
	
	struct vm_area_struct vmas[] = {
		(struct vm_area_struct) {.vm_start = 10, .vm_end = 20},
		(struct vm_area_struct) {.vm_start = 20, .vm_end = 25},
		(struct vm_area_struct) {.vm_start = 200, .vm_end = 210},
		(struct vm_area_struct) {.vm_start = 210, .vm_end = 220},
		(struct vm_area_struct) {.vm_start = 300, .vm_end = 305},
		(struct vm_area_struct) {.vm_start = 307, .vm_end = 330},
	};

	mt_init_flags(&mm.mm_mt, MM_MT_FLAGS);
	if (__link_vmas(&mm.mm_mt, vmas, ARRAY_SIZE(vmas)))
		kunit_skip(test, "Failed to create VMA tree");

	__damon_va_three_regions(&mm, regions);

	KUNIT_EXPECT_EQ(test, 10ul, regions[0].start);
	KUNIT_EXPECT_EQ(test, 25ul, regions[0].end);
	KUNIT_EXPECT_EQ(test, 200ul, regions[1].start);
	KUNIT_EXPECT_EQ(test, 220ul, regions[1].end);
	KUNIT_EXPECT_EQ(test, 300ul, regions[2].start);
	KUNIT_EXPECT_EQ(test, 330ul, regions[2].end);
}

static struct damon_region *__nth_region_of(struct damon_target *t, int idx)
{
	struct damon_region *r;
	unsigned int i = 0;

	damon_for_each_region(r, t) {
		if (i++ == idx)
			return r;
	}

	return NULL;
}


static void damon_do_test_apply_three_regions(struct kunit *test,
				unsigned long *regions, int nr_regions,
				struct damon_addr_range *three_regions,
				unsigned long *expected, int nr_expected)
{
	struct damon_target *t;
	struct damon_region *r;
	int i;

	t = damon_new_target();
	for (i = 0; i < nr_regions / 2; i++) {
		r = damon_new_region(regions[i * 2], regions[i * 2 + 1]);
		damon_add_region(r, t);
	}

	damon_set_regions(t, three_regions, 3);

	for (i = 0; i < nr_expected / 2; i++) {
		r = __nth_region_of(t, i);
		KUNIT_EXPECT_EQ(test, r->ar.start, expected[i * 2]);
		KUNIT_EXPECT_EQ(test, r->ar.end, expected[i * 2 + 1]);
	}

	damon_destroy_target(t);
}


static void damon_test_apply_three_regions1(struct kunit *test)
{
	
	unsigned long regions[] = {10, 20, 20, 30, 50, 55, 55, 57, 57, 59,
				70, 80, 80, 90, 90, 100};
	
	struct damon_addr_range new_three_regions[3] = {
		(struct damon_addr_range){.start = 5, .end = 27},
		(struct damon_addr_range){.start = 45, .end = 55},
		(struct damon_addr_range){.start = 73, .end = 104} };
	
	unsigned long expected[] = {5, 20, 20, 27, 45, 55,
				73, 80, 80, 90, 90, 104};

	damon_do_test_apply_three_regions(test, regions, ARRAY_SIZE(regions),
			new_three_regions, expected, ARRAY_SIZE(expected));
}


static void damon_test_apply_three_regions2(struct kunit *test)
{
	
	unsigned long regions[] = {10, 20, 20, 30, 50, 55, 55, 57, 57, 59,
				70, 80, 80, 90, 90, 100};
	
	struct damon_addr_range new_three_regions[3] = {
		(struct damon_addr_range){.start = 5, .end = 27},
		(struct damon_addr_range){.start = 56, .end = 57},
		(struct damon_addr_range){.start = 65, .end = 104} };
	
	unsigned long expected[] = {5, 20, 20, 27, 56, 57,
				65, 80, 80, 90, 90, 104};

	damon_do_test_apply_three_regions(test, regions, ARRAY_SIZE(regions),
			new_three_regions, expected, ARRAY_SIZE(expected));
}


static void damon_test_apply_three_regions3(struct kunit *test)
{
	
	unsigned long regions[] = {10, 20, 20, 30, 50, 55, 55, 57, 57, 59,
				70, 80, 80, 90, 90, 100};
	
	struct damon_addr_range new_three_regions[3] = {
		(struct damon_addr_range){.start = 5, .end = 27},
		(struct damon_addr_range){.start = 61, .end = 63},
		(struct damon_addr_range){.start = 65, .end = 104} };
	
	unsigned long expected[] = {5, 20, 20, 27, 61, 63,
				65, 80, 80, 90, 90, 104};

	damon_do_test_apply_three_regions(test, regions, ARRAY_SIZE(regions),
			new_three_regions, expected, ARRAY_SIZE(expected));
}


static void damon_test_apply_three_regions4(struct kunit *test)
{
	
	unsigned long regions[] = {10, 20, 20, 30, 50, 55, 55, 57, 57, 59,
				70, 80, 80, 90, 90, 100};
	
	struct damon_addr_range new_three_regions[3] = {
		(struct damon_addr_range){.start = 5, .end = 7},
		(struct damon_addr_range){.start = 30, .end = 32},
		(struct damon_addr_range){.start = 65, .end = 68} };
	
	unsigned long expected[] = {5, 7, 30, 32, 65, 68};

	damon_do_test_apply_three_regions(test, regions, ARRAY_SIZE(regions),
			new_three_regions, expected, ARRAY_SIZE(expected));
}

static void damon_test_split_evenly_fail(struct kunit *test,
		unsigned long start, unsigned long end, unsigned int nr_pieces)
{
	struct damon_target *t = damon_new_target();
	struct damon_region *r = damon_new_region(start, end);

	damon_add_region(r, t);
	KUNIT_EXPECT_EQ(test,
			damon_va_evenly_split_region(t, r, nr_pieces), -EINVAL);
	KUNIT_EXPECT_EQ(test, damon_nr_regions(t), 1u);

	damon_for_each_region(r, t) {
		KUNIT_EXPECT_EQ(test, r->ar.start, start);
		KUNIT_EXPECT_EQ(test, r->ar.end, end);
	}

	damon_free_target(t);
}

static void damon_test_split_evenly_succ(struct kunit *test,
	unsigned long start, unsigned long end, unsigned int nr_pieces)
{
	struct damon_target *t = damon_new_target();
	struct damon_region *r = damon_new_region(start, end);
	unsigned long expected_width = (end - start) / nr_pieces;
	unsigned long i = 0;

	damon_add_region(r, t);
	KUNIT_EXPECT_EQ(test,
			damon_va_evenly_split_region(t, r, nr_pieces), 0);
	KUNIT_EXPECT_EQ(test, damon_nr_regions(t), nr_pieces);

	damon_for_each_region(r, t) {
		if (i == nr_pieces - 1) {
			KUNIT_EXPECT_EQ(test,
				r->ar.start, start + i * expected_width);
			KUNIT_EXPECT_EQ(test, r->ar.end, end);
			break;
		}
		KUNIT_EXPECT_EQ(test,
				r->ar.start, start + i++ * expected_width);
		KUNIT_EXPECT_EQ(test, r->ar.end, start + i * expected_width);
	}
	damon_free_target(t);
}

static void damon_test_split_evenly(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, damon_va_evenly_split_region(NULL, NULL, 5),
			-EINVAL);

	damon_test_split_evenly_fail(test, 0, 100, 0);
	damon_test_split_evenly_succ(test, 0, 100, 10);
	damon_test_split_evenly_succ(test, 5, 59, 5);
	damon_test_split_evenly_fail(test, 5, 6, 2);
}

static struct kunit_case damon_test_cases[] = {
	KUNIT_CASE(damon_test_three_regions_in_vmas),
	KUNIT_CASE(damon_test_apply_three_regions1),
	KUNIT_CASE(damon_test_apply_three_regions2),
	KUNIT_CASE(damon_test_apply_three_regions3),
	KUNIT_CASE(damon_test_apply_three_regions4),
	KUNIT_CASE(damon_test_split_evenly),
	{},
};

static struct kunit_suite damon_test_suite = {
	.name = "damon-operations",
	.test_cases = damon_test_cases,
};
kunit_test_suite(damon_test_suite);

#endif 

#endif	
