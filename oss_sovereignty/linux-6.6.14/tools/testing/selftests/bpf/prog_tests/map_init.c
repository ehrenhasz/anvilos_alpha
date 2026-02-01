
 
} __bpf_percpu_val_align pcpu_map_value_t;


static int map_populate(int map_fd, int num)
{
	pcpu_map_value_t value[nr_cpus];
	int i, err;
	map_key_t key;

	for (i = 0; i < nr_cpus; i++)
		bpf_percpu(value, i) = FILL_VALUE;

	for (key = 1; key <= num; key++) {
		err = bpf_map_update_elem(map_fd, &key, value, BPF_NOEXIST);
		if (!ASSERT_OK(err, "bpf_map_update_elem"))
			return -1;
	}

	return 0;
}

static struct test_map_init *setup(enum bpf_map_type map_type, int map_sz,
			    int *map_fd, int populate)
{
	struct test_map_init *skel;
	int err;

	skel = test_map_init__open();
	if (!ASSERT_OK_PTR(skel, "skel_open"))
		return NULL;

	err = bpf_map__set_type(skel->maps.hashmap1, map_type);
	if (!ASSERT_OK(err, "bpf_map__set_type"))
		goto error;

	err = bpf_map__set_max_entries(skel->maps.hashmap1, map_sz);
	if (!ASSERT_OK(err, "bpf_map__set_max_entries"))
		goto error;

	err = test_map_init__load(skel);
	if (!ASSERT_OK(err, "skel_load"))
		goto error;

	*map_fd = bpf_map__fd(skel->maps.hashmap1);
	if (CHECK(*map_fd < 0, "bpf_map__fd", "failed\n"))
		goto error;

	err = map_populate(*map_fd, populate);
	if (!ASSERT_OK(err, "map_populate"))
		goto error_map;

	return skel;

error_map:
	close(*map_fd);
error:
	test_map_init__destroy(skel);
	return NULL;
}

 
static int prog_run_insert_elem(struct test_map_init *skel, map_key_t key,
				map_value_t value)
{
	struct test_map_init__bss *bss;

	bss = skel->bss;

	bss->inKey = key;
	bss->inValue = value;
	bss->inPid = getpid();

	if (!ASSERT_OK(test_map_init__attach(skel), "skel_attach"))
		return -1;

	 
	syscall(__NR_getpgid);

	test_map_init__detach(skel);

	return 0;
}

static int check_values_one_cpu(pcpu_map_value_t *value, map_value_t expected)
{
	int i, nzCnt = 0;
	map_value_t val;

	for (i = 0; i < nr_cpus; i++) {
		val = bpf_percpu(value, i);
		if (val) {
			if (CHECK(val != expected, "map value",
				  "unexpected for cpu %d: 0x%llx\n", i, val))
				return -1;
			nzCnt++;
		}
	}

	if (CHECK(nzCnt != 1, "map value", "set for %d CPUs instead of 1!\n",
		  nzCnt))
		return -1;

	return 0;
}

 
static void test_pcpu_map_init(void)
{
	pcpu_map_value_t value[nr_cpus];
	struct test_map_init *skel;
	int map_fd, err;
	map_key_t key;

	 
	skel = setup(BPF_MAP_TYPE_PERCPU_HASH, 1, &map_fd, 1);
	if (!ASSERT_OK_PTR(skel, "prog_setup"))
		return;

	 
	key = 1;
	err = bpf_map_delete_elem(map_fd, &key);
	if (!ASSERT_OK(err, "bpf_map_delete_elem"))
		goto cleanup;

	 
	err = prog_run_insert_elem(skel, key, TEST_VALUE);
	if (!ASSERT_OK(err, "prog_run_insert_elem"))
		goto cleanup;

	 
	err = bpf_map_lookup_elem(map_fd, &key, value);
	if (!ASSERT_OK(err, "bpf_map_lookup_elem"))
		goto cleanup;

	 
	check_values_one_cpu(value, TEST_VALUE);

cleanup:
	test_map_init__destroy(skel);
}

 
static void test_pcpu_lru_map_init(void)
{
	pcpu_map_value_t value[nr_cpus];
	struct test_map_init *skel;
	int map_fd, err;
	map_key_t key;

	 
	skel = setup(BPF_MAP_TYPE_LRU_PERCPU_HASH, 2, &map_fd, 2);
	if (!ASSERT_OK_PTR(skel, "prog_setup"))
		return;

	 
	key = 3;
	err = prog_run_insert_elem(skel, key, TEST_VALUE);
	if (!ASSERT_OK(err, "prog_run_insert_elem"))
		goto cleanup;

	 
	err = bpf_map_lookup_elem(map_fd, &key, value);
	if (!ASSERT_OK(err, "bpf_map_lookup_elem"))
		goto cleanup;

	 
	check_values_one_cpu(value, TEST_VALUE);

cleanup:
	test_map_init__destroy(skel);
}

void test_map_init(void)
{
	nr_cpus = bpf_num_possible_cpus();
	if (nr_cpus <= 1) {
		printf("%s:SKIP: >1 cpu needed for this test\n", __func__);
		test__skip();
		return;
	}

	if (test__start_subtest("pcpu_map_init"))
		test_pcpu_map_init();
	if (test__start_subtest("pcpu_lru_map_init"))
		test_pcpu_lru_map_init();
}
