#ifndef _BPF_TESTMOD_H
#define _BPF_TESTMOD_H
#include <linux/types.h>
struct bpf_testmod_test_read_ctx {
	char *buf;
	loff_t off;
	size_t len;
};
struct bpf_testmod_test_write_ctx {
	char *buf;
	loff_t off;
	size_t len;
};
struct bpf_testmod_test_writable_ctx {
	bool early_ret;
	int val;
};
struct bpf_iter_testmod_seq {
	s64 value;
	int cnt;
};
#endif  
