


#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char _license[] SEC("license") = "GPL";

struct {
	char in[256];
	char out[256];
} data = {};

struct core_reloc_flavors {
	int a;
	int b;
	int c;
};

 
struct core_reloc_flavors___reversed {
	int c;
	int b;
	int a;
};

 
struct core_reloc_flavors___weird {
	struct {
		int b;
	};
	 
	union {
		int a;
		int c;
	};
};

#define CORE_READ(dst, src) bpf_core_read(dst, sizeof(*(dst)), src)

SEC("raw_tracepoint/sys_enter")
int test_core_flavors(void *ctx)
{
	struct core_reloc_flavors *in_orig = (void *)&data.in;
	struct core_reloc_flavors___reversed *in_rev = (void *)&data.in;
	struct core_reloc_flavors___weird *in_weird = (void *)&data.in;
	struct core_reloc_flavors *out = (void *)&data.out;

	 
	if (CORE_READ(&out->a, &in_weird->a))
		return 1;
	 
	if (CORE_READ(&out->b, &in_rev->b))
		return 1;
	 
	if (CORE_READ(&out->c, &in_orig->c))
		return 1;

	return 0;
}

