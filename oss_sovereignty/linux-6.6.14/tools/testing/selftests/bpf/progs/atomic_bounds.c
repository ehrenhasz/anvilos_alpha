
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <stdbool.h>

#ifdef ENABLE_ATOMICS_TESTS
bool skip_tests __attribute((__section__(".data"))) = false;
#else
bool skip_tests = true;
#endif

SEC("fentry/bpf_fentry_test1")
int BPF_PROG(sub, int x)
{
#ifdef ENABLE_ATOMICS_TESTS
	int a = 0;
	int b = __sync_fetch_and_add(&a, 1);
	 
	while (b)
		continue;
#endif
	return 0;
}
