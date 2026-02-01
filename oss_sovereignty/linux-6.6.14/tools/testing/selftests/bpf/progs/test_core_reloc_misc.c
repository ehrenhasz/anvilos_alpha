


#include <linux/bpf.h>
#include <stdint.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char _license[] SEC("license") = "GPL";

struct {
	char in[256];
	char out[256];
} data = {};

struct core_reloc_misc_output {
	int a, b, c;
};

struct core_reloc_misc___a {
	int a1;
	int a2;
};

struct core_reloc_misc___b {
	int b1;
	int b2;
};

 
struct core_reloc_misc_extensible {
	int a;
	int b;
};

#define CORE_READ(dst, src) bpf_core_read(dst, sizeof(*(dst)), src)

SEC("raw_tracepoint/sys_enter")
int test_core_misc(void *ctx)
{
	struct core_reloc_misc___a *in_a = (void *)&data.in;
	struct core_reloc_misc___b *in_b = (void *)&data.in;
	struct core_reloc_misc_extensible *in_ext = (void *)&data.in;
	struct core_reloc_misc_output *out = (void *)&data.out;

	 
	if (CORE_READ(&out->a, &in_a->a1) ||		 
	    CORE_READ(&out->b, &in_b->b1))		 
		return 1;

	  
	if (CORE_READ(&out->c, &in_ext[2]))		 
		return 1;

	return 0;
}

