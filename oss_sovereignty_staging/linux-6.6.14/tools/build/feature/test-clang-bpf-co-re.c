


struct test {
	int a;
	int b;
} __attribute__((preserve_access_index));

volatile struct test global_value_for_test = {};
