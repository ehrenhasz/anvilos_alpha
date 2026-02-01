
 
#include <errno.h>
#include <sys/syscall.h>
#include <unistd.h>
#include "test_global_map_resize.skel.h"
#include "test_progs.h"

static void run_prog_bss_array_sum(void)
{
	(void)syscall(__NR_getpid);
}

static void run_prog_data_array_sum(void)
{
	(void)syscall(__NR_getuid);
}

static void global_map_resize_bss_subtest(void)
{
	int err;
	struct test_global_map_resize *skel;
	struct bpf_map *map;
	const __u32 desired_sz = sizeof(skel->bss->sum) + sysconf(_SC_PAGE_SIZE) * 2;
	size_t array_len, actual_sz, new_sz;

	skel = test_global_map_resize__open();
	if (!ASSERT_OK_PTR(skel, "test_global_map_resize__open"))
		goto teardown;

	 
	skel->bss->array[0] = 1;

	 
	map = skel->maps.bss;
	err = bpf_map__set_value_size(map, desired_sz);
	if (!ASSERT_OK(err, "bpf_map__set_value_size"))
		goto teardown;
	if (!ASSERT_EQ(bpf_map__value_size(map), desired_sz, "resize"))
		goto teardown;

	new_sz = sizeof(skel->data_percpu_arr->percpu_arr[0]) * libbpf_num_possible_cpus();
	err = bpf_map__set_value_size(skel->maps.data_percpu_arr, new_sz);
	ASSERT_OK(err, "percpu_arr_resize");

	 
	array_len = (desired_sz - sizeof(skel->bss->sum)) / sizeof(skel->bss->array[0]);
	if (!ASSERT_GT(array_len, 1, "array_len"))
		goto teardown;

	skel->bss = bpf_map__initial_value(skel->maps.bss, &actual_sz);
	if (!ASSERT_OK_PTR(skel->bss, "bpf_map__initial_value (ptr)"))
		goto teardown;
	if (!ASSERT_EQ(actual_sz, desired_sz, "bpf_map__initial_value (size)"))
		goto teardown;

	 
	for (int i = 1; i < array_len; i++)
		skel->bss->array[i] = 1;

	 
	skel->rodata->pid = getpid();
	skel->rodata->bss_array_len = array_len;
	skel->rodata->data_array_len = 1;

	err = test_global_map_resize__load(skel);
	if (!ASSERT_OK(err, "test_global_map_resize__load"))
		goto teardown;
	err = test_global_map_resize__attach(skel);
	if (!ASSERT_OK(err, "test_global_map_resize__attach"))
		goto teardown;

	 
	run_prog_bss_array_sum();
	if (!ASSERT_EQ(skel->bss->sum, array_len, "sum"))
		goto teardown;

teardown:
	test_global_map_resize__destroy(skel);
}

static void global_map_resize_data_subtest(void)
{
	struct test_global_map_resize *skel;
	struct bpf_map *map;
	const __u32 desired_sz = sysconf(_SC_PAGE_SIZE) * 2;
	size_t array_len, actual_sz, new_sz;
	int err;

	skel = test_global_map_resize__open();
	if (!ASSERT_OK_PTR(skel, "test_global_map_resize__open"))
		goto teardown;

	 
	skel->data_custom->my_array[0] = 1;

	 
	map = skel->maps.data_custom;
	err = bpf_map__set_value_size(map, desired_sz);
	if (!ASSERT_OK(err, "bpf_map__set_value_size"))
		goto teardown;
	if (!ASSERT_EQ(bpf_map__value_size(map), desired_sz, "resize"))
		goto teardown;

	new_sz = sizeof(skel->data_percpu_arr->percpu_arr[0]) * libbpf_num_possible_cpus();
	err = bpf_map__set_value_size(skel->maps.data_percpu_arr, new_sz);
	ASSERT_OK(err, "percpu_arr_resize");

	 
	array_len = (desired_sz - sizeof(skel->bss->sum)) / sizeof(skel->data_custom->my_array[0]);
	if (!ASSERT_GT(array_len, 1, "array_len"))
		goto teardown;

	skel->data_custom = bpf_map__initial_value(skel->maps.data_custom, &actual_sz);
	if (!ASSERT_OK_PTR(skel->data_custom, "bpf_map__initial_value (ptr)"))
		goto teardown;
	if (!ASSERT_EQ(actual_sz, desired_sz, "bpf_map__initial_value (size)"))
		goto teardown;

	 
	for (int i = 1; i < array_len; i++)
		skel->data_custom->my_array[i] = 1;

	 
	skel->rodata->pid = getpid();
	skel->rodata->bss_array_len = 1;
	skel->rodata->data_array_len = array_len;

	err = test_global_map_resize__load(skel);
	if (!ASSERT_OK(err, "test_global_map_resize__load"))
		goto teardown;
	err = test_global_map_resize__attach(skel);
	if (!ASSERT_OK(err, "test_global_map_resize__attach"))
		goto teardown;

	 
	run_prog_data_array_sum();
	if (!ASSERT_EQ(skel->bss->sum, array_len, "sum"))
		goto teardown;

teardown:
	test_global_map_resize__destroy(skel);
}

static void global_map_resize_invalid_subtest(void)
{
	int err;
	struct test_global_map_resize *skel;
	struct bpf_map *map;
	__u32 element_sz, desired_sz;

	skel = test_global_map_resize__open();
	if (!ASSERT_OK_PTR(skel, "test_global_map_resize__open"))
		return;

	  
	map = skel->maps.data_custom;
	if (!ASSERT_NEQ(bpf_map__btf_value_type_id(map), 0, ".data.custom initial btf"))
		goto teardown;
	 
	element_sz = sizeof(skel->data_custom->my_array[0]);
	desired_sz = element_sz + element_sz / 2;
	 
	if (!ASSERT_NEQ(desired_sz % element_sz, 0, "my_array alignment"))
		goto teardown;
	err = bpf_map__set_value_size(map, desired_sz);
	 
	if (!ASSERT_OK(err, ".data.custom bpf_map__set_value_size") ||
	    !ASSERT_EQ(bpf_map__btf_key_type_id(map), 0, ".data.custom clear btf key") ||
	    !ASSERT_EQ(bpf_map__btf_value_type_id(map), 0, ".data.custom clear btf val"))
		goto teardown;

	 
	map = skel->maps.data_non_array;
	if (!ASSERT_NEQ(bpf_map__btf_value_type_id(map), 0, ".data.non_array initial btf"))
		goto teardown;
	 
	desired_sz = 1024;
	err = bpf_map__set_value_size(map, desired_sz);
	 
	if (!ASSERT_OK(err, ".data.non_array bpf_map__set_value_size") ||
	    !ASSERT_EQ(bpf_map__btf_key_type_id(map), 0, ".data.non_array clear btf key") ||
	    !ASSERT_EQ(bpf_map__btf_value_type_id(map), 0, ".data.non_array clear btf val"))
		goto teardown;

	 
	map = skel->maps.data_array_not_last;
	if (!ASSERT_NEQ(bpf_map__btf_value_type_id(map), 0, ".data.array_not_last initial btf"))
		goto teardown;
	 
	element_sz = sizeof(skel->data_array_not_last->my_array_first[0]);
	desired_sz = element_sz * 8;
	 
	if (!ASSERT_EQ(desired_sz % element_sz, 0, "my_array_first alignment"))
		goto teardown;
	err = bpf_map__set_value_size(map, desired_sz);
	 
	if (!ASSERT_OK(err, ".data.array_not_last bpf_map__set_value_size") ||
	    !ASSERT_EQ(bpf_map__btf_key_type_id(map), 0, ".data.array_not_last clear btf key") ||
	    !ASSERT_EQ(bpf_map__btf_value_type_id(map), 0, ".data.array_not_last clear btf val"))
		goto teardown;

teardown:
	test_global_map_resize__destroy(skel);
}

void test_global_map_resize(void)
{
	if (test__start_subtest("global_map_resize_bss"))
		global_map_resize_bss_subtest();

	if (test__start_subtest("global_map_resize_data"))
		global_map_resize_data_subtest();

	if (test__start_subtest("global_map_resize_invalid"))
		global_map_resize_invalid_subtest();
}
