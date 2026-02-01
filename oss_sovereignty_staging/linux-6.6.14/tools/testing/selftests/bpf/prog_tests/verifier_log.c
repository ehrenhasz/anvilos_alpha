
 

#include <test_progs.h>
#include <bpf/btf.h>

#include "test_log_buf.skel.h"


static bool check_prog_load(int prog_fd, bool expect_err, const char *tag)
{
	if (expect_err) {
		if (!ASSERT_LT(prog_fd, 0, tag)) {
			close(prog_fd);
			return false;
		}
	} else   {
		if (!ASSERT_GT(prog_fd, 0, tag))
			return false;
	}
	if (prog_fd >= 0)
		close(prog_fd);
	return true;
}

static struct {
	 
	char filler[1024];
	char buf[1024];
	 
	char reference[1024];
} logs;
static const struct bpf_insn *insns;
static size_t insn_cnt;

static int load_prog(struct bpf_prog_load_opts *opts, bool expect_load_error)
{
	int prog_fd;

	prog_fd = bpf_prog_load(BPF_PROG_TYPE_RAW_TRACEPOINT, "log_prog",
				"GPL", insns, insn_cnt, opts);
	check_prog_load(prog_fd, expect_load_error, "prog_load");

	return prog_fd;
}

static void verif_log_subtest(const char *name, bool expect_load_error, int log_level)
{
	LIBBPF_OPTS(bpf_prog_load_opts, opts);
	char *exp_log, prog_name[16], op_name[32];
	struct test_log_buf *skel;
	struct bpf_program *prog;
	size_t fixed_log_sz;
	__u32 log_true_sz_fixed, log_true_sz_rolling;
	int i, mode, err, prog_fd, res;

	skel = test_log_buf__open();
	if (!ASSERT_OK_PTR(skel, "skel_open"))
		return;

	bpf_object__for_each_program(prog, skel->obj) {
		if (strcmp(bpf_program__name(prog), name) == 0)
			bpf_program__set_autoload(prog, true);
		else
			bpf_program__set_autoload(prog, false);
	}

	err = test_log_buf__load(skel);
	if (!expect_load_error && !ASSERT_OK(err, "unexpected_load_failure"))
		goto cleanup;
	if (expect_load_error && !ASSERT_ERR(err, "unexpected_load_success"))
		goto cleanup;

	insns = bpf_program__insns(skel->progs.good_prog);
	insn_cnt = bpf_program__insn_cnt(skel->progs.good_prog);

	opts.log_buf = logs.reference;
	opts.log_size = sizeof(logs.reference);
	opts.log_level = log_level | 8  ;
	load_prog(&opts, expect_load_error);

	fixed_log_sz = strlen(logs.reference) + 1;
	if (!ASSERT_GT(fixed_log_sz, 50, "fixed_log_sz"))
		goto cleanup;
	memset(logs.reference + fixed_log_sz, 0, sizeof(logs.reference) - fixed_log_sz);

	 
	if (log_level >= 2 || expect_load_error) {
		opts.log_buf = logs.buf;
		opts.log_level = log_level | 8;  
		opts.log_size = 25;

		prog_fd = bpf_prog_load(BPF_PROG_TYPE_RAW_TRACEPOINT, "log_fixed25",
					"GPL", insns, insn_cnt, &opts);
		if (!ASSERT_EQ(prog_fd, -ENOSPC, "unexpected_log_fixed_prog_load_result")) {
			if (prog_fd >= 0)
				close(prog_fd);
			goto cleanup;
		}
		if (!ASSERT_EQ(strlen(logs.buf), 24, "log_fixed_25"))
			goto cleanup;
		if (!ASSERT_STRNEQ(logs.buf, logs.reference, 24, "log_fixed_contents_25"))
			goto cleanup;
	}

	 
	opts.log_buf = logs.buf;

	 
	for (mode = 1; mode >= 0; mode--) {
		 
		memset(logs.filler, 'A', sizeof(logs.filler));
		logs.filler[sizeof(logs.filler) - 1] = '\0';
		memset(logs.buf, 'A', sizeof(logs.buf));
		logs.buf[sizeof(logs.buf) - 1] = '\0';

		for (i = 1; i < fixed_log_sz; i++) {
			opts.log_size = i;
			opts.log_level = log_level | (mode ? 0 : 8  );

			snprintf(prog_name, sizeof(prog_name),
				 "log_%s_%d", mode ? "roll" : "fixed", i);
			prog_fd = bpf_prog_load(BPF_PROG_TYPE_RAW_TRACEPOINT, prog_name,
						"GPL", insns, insn_cnt, &opts);

			snprintf(op_name, sizeof(op_name),
				 "log_%s_prog_load_%d", mode ? "roll" : "fixed", i);
			if (!ASSERT_EQ(prog_fd, -ENOSPC, op_name)) {
				if (prog_fd >= 0)
					close(prog_fd);
				goto cleanup;
			}

			snprintf(op_name, sizeof(op_name),
				 "log_%s_strlen_%d", mode ? "roll" : "fixed", i);
			ASSERT_EQ(strlen(logs.buf), i - 1, op_name);

			if (mode)
				exp_log = logs.reference + fixed_log_sz - i;
			else
				exp_log = logs.reference;

			snprintf(op_name, sizeof(op_name),
				 "log_%s_contents_%d", mode ? "roll" : "fixed", i);
			if (!ASSERT_STRNEQ(logs.buf, exp_log, i - 1, op_name)) {
				printf("CMP:%d\nS1:'%s'\nS2:'%s'\n",
					strncmp(logs.buf, exp_log, i - 1),
					logs.buf, exp_log);
				goto cleanup;
			}

			 
			snprintf(op_name, sizeof(op_name),
				 "log_%s_unused_%d", mode ? "roll" : "fixed", i);
			if (!ASSERT_STREQ(logs.buf + i, logs.filler + i, op_name)) {
				printf("CMP:%d\nS1:'%s'\nS2:'%s'\n",
					strcmp(logs.buf + i, logs.filler + i),
					logs.buf + i, logs.filler + i);
				goto cleanup;
			}
		}
	}

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level | 8;  
	opts.log_size = sizeof(logs.buf);
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_fixed");

	log_true_sz_fixed = opts.log_true_size;
	ASSERT_GT(log_true_sz_fixed, 0, "log_true_sz_fixed");

	 
	opts.log_buf = NULL;
	opts.log_level = log_level | 8;  
	opts.log_size = 0;
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_fixed_null");
	ASSERT_EQ(opts.log_true_size, log_true_sz_fixed, "log_sz_fixed_null_eq");

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level;
	opts.log_size = sizeof(logs.buf);
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_rolling");

	log_true_sz_rolling = opts.log_true_size;
	ASSERT_EQ(log_true_sz_rolling, log_true_sz_fixed, "log_true_sz_eq");

	 
	opts.log_buf = NULL;
	opts.log_level = log_level;
	opts.log_size = 0;
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_rolling_null");
	ASSERT_EQ(opts.log_true_size, log_true_sz_rolling, "log_true_sz_null_eq");

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level | 8;  
	opts.log_size = log_true_sz_fixed - 1;
	opts.log_true_size = 0;
	res = load_prog(&opts, true  );
	ASSERT_EQ(res, -ENOSPC, "prog_load_res_too_short_fixed");

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level | 8;  
	opts.log_size = log_true_sz_fixed;
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_just_right_fixed");

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level;
	opts.log_size = log_true_sz_rolling - 1;
	res = load_prog(&opts, true  );
	ASSERT_EQ(res, -ENOSPC, "prog_load_res_too_short_rolling");

	 
	opts.log_buf = logs.buf;
	opts.log_level = log_level;
	opts.log_size = log_true_sz_rolling;
	opts.log_true_size = 0;
	res = load_prog(&opts, expect_load_error);
	ASSERT_NEQ(res, -ENOSPC, "prog_load_res_just_right_rolling");

cleanup:
	test_log_buf__destroy(skel);
}

static const void *btf_data;
static u32 btf_data_sz;

static int load_btf(struct bpf_btf_load_opts *opts, bool expect_err)
{
	int fd;

	fd = bpf_btf_load(btf_data, btf_data_sz, opts);
	if (fd >= 0)
		close(fd);
	if (expect_err)
		ASSERT_LT(fd, 0, "btf_load_failure");
	else  
		ASSERT_GT(fd, 0, "btf_load_success");
	return fd;
}

static void verif_btf_log_subtest(bool bad_btf)
{
	LIBBPF_OPTS(bpf_btf_load_opts, opts);
	struct btf *btf;
	struct btf_type *t;
	char *exp_log, op_name[32];
	size_t fixed_log_sz;
	__u32 log_true_sz_fixed, log_true_sz_rolling;
	int i, res;

	 
	btf = btf__new_empty();
	if (!ASSERT_OK_PTR(btf, "btf_new_empty"))
		return;
	res = btf__add_int(btf, "whatever", 4, 0);
	if (!ASSERT_GT(res, 0, "btf_add_int_id"))
		goto cleanup;
	if (bad_btf) {
		 
		t = (void *)btf__type_by_id(btf, res);
		if (!ASSERT_OK_PTR(t, "int_btf_type"))
			goto cleanup;
		t->size = 3;
	}

	btf_data = btf__raw_data(btf, &btf_data_sz);
	if (!ASSERT_OK_PTR(btf_data, "btf_data"))
		goto cleanup;

	load_btf(&opts, bad_btf);

	opts.log_buf = logs.reference;
	opts.log_size = sizeof(logs.reference);
	opts.log_level = 1 | 8  ;
	load_btf(&opts, bad_btf);

	fixed_log_sz = strlen(logs.reference) + 1;
	if (!ASSERT_GT(fixed_log_sz, 50, "fixed_log_sz"))
		goto cleanup;
	memset(logs.reference + fixed_log_sz, 0, sizeof(logs.reference) - fixed_log_sz);

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1 | 8;  
	opts.log_size = 25;
	res = load_btf(&opts, true);
	ASSERT_EQ(res, -ENOSPC, "half_log_fd");
	ASSERT_EQ(strlen(logs.buf), 24, "log_fixed_25");
	ASSERT_STRNEQ(logs.buf, logs.reference, 24, op_name);

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1;  

	 
	memset(logs.filler, 'A', sizeof(logs.filler));
	logs.filler[sizeof(logs.filler) - 1] = '\0';
	memset(logs.buf, 'A', sizeof(logs.buf));
	logs.buf[sizeof(logs.buf) - 1] = '\0';

	for (i = 1; i < fixed_log_sz; i++) {
		opts.log_size = i;

		snprintf(op_name, sizeof(op_name), "log_roll_btf_load_%d", i);
		res = load_btf(&opts, true);
		if (!ASSERT_EQ(res, -ENOSPC, op_name))
			goto cleanup;

		exp_log = logs.reference + fixed_log_sz - i;
		snprintf(op_name, sizeof(op_name), "log_roll_contents_%d", i);
		if (!ASSERT_STREQ(logs.buf, exp_log, op_name)) {
			printf("CMP:%d\nS1:'%s'\nS2:'%s'\n",
				strcmp(logs.buf, exp_log),
				logs.buf, exp_log);
			goto cleanup;
		}

		 
		snprintf(op_name, sizeof(op_name), "log_roll_unused_tail_%d", i);
		if (!ASSERT_STREQ(logs.buf + i, logs.filler + i, op_name)) {
			printf("CMP:%d\nS1:'%s'\nS2:'%s'\n",
				strcmp(logs.buf + i, logs.filler + i),
				logs.buf + i, logs.filler + i);
			goto cleanup;
		}
	}

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1 | 8;  
	opts.log_size = sizeof(logs.buf);
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_fixed");

	log_true_sz_fixed = opts.log_true_size;
	ASSERT_GT(log_true_sz_fixed, 0, "log_true_sz_fixed");

	 
	opts.log_buf = NULL;
	opts.log_level = 1 | 8;  
	opts.log_size = 0;
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_fixed_null");
	ASSERT_EQ(opts.log_true_size, log_true_sz_fixed, "log_sz_fixed_null_eq");

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1;
	opts.log_size = sizeof(logs.buf);
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_rolling");

	log_true_sz_rolling = opts.log_true_size;
	ASSERT_EQ(log_true_sz_rolling, log_true_sz_fixed, "log_true_sz_eq");

	 
	opts.log_buf = NULL;
	opts.log_level = 1;
	opts.log_size = 0;
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_rolling_null");
	ASSERT_EQ(opts.log_true_size, log_true_sz_rolling, "log_true_sz_null_eq");

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1 | 8;  
	opts.log_size = log_true_sz_fixed - 1;
	opts.log_true_size = 0;
	res = load_btf(&opts, true);
	ASSERT_EQ(res, -ENOSPC, "btf_load_res_too_short_fixed");

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1 | 8;  
	opts.log_size = log_true_sz_fixed;
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_just_right_fixed");

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1;
	opts.log_size = log_true_sz_rolling - 1;
	res = load_btf(&opts, true);
	ASSERT_EQ(res, -ENOSPC, "btf_load_res_too_short_rolling");

	 
	opts.log_buf = logs.buf;
	opts.log_level = 1;
	opts.log_size = log_true_sz_rolling;
	opts.log_true_size = 0;
	res = load_btf(&opts, bad_btf);
	ASSERT_NEQ(res, -ENOSPC, "btf_load_res_just_right_rolling");

cleanup:
	btf__free(btf);
}

void test_verifier_log(void)
{
	if (test__start_subtest("good_prog-level1"))
		verif_log_subtest("good_prog", false, 1);
	if (test__start_subtest("good_prog-level2"))
		verif_log_subtest("good_prog", false, 2);
	if (test__start_subtest("bad_prog-level1"))
		verif_log_subtest("bad_prog", true, 1);
	if (test__start_subtest("bad_prog-level2"))
		verif_log_subtest("bad_prog", true, 2);
	if (test__start_subtest("bad_btf"))
		verif_btf_log_subtest(true  );
	if (test__start_subtest("good_btf"))
		verif_btf_log_subtest(false  );
}
