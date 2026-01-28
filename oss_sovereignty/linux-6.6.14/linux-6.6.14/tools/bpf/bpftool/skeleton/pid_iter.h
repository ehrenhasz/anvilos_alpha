#ifndef __PID_ITER_H
#define __PID_ITER_H
struct pid_iter_entry {
	__u32 id;
	int pid;
	__u64 bpf_cookie;
	bool has_bpf_cookie;
	char comm[16];
};
#endif
