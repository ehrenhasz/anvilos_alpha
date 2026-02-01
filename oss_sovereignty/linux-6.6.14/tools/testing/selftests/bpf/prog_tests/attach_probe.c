
#include <test_progs.h>
#include "test_attach_kprobe_sleepable.skel.h"
#include "test_attach_probe_manual.skel.h"
#include "test_attach_probe.skel.h"

 
volatile unsigned short uprobe_ref_ctr __attribute__((unused)) __attribute((section(".probes")));

 
static noinline void trigger_func(void)
{
	asm volatile ("");
}

 
static noinline void trigger_func2(void)
{
	asm volatile ("");
}

 
static noinline void trigger_func3(void)
{
	asm volatile ("");
}

 
static noinline void trigger_func4(void)
{
	asm volatile ("");
}

static char test_data[] = "test_data";

 
static void test_attach_probe_manual(enum probe_attach_mode attach_mode)
{
	DECLARE_LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts);
	DECLARE_LIBBPF_OPTS(bpf_kprobe_opts, kprobe_opts);
	struct bpf_link *kprobe_link, *kretprobe_link;
	struct bpf_link *uprobe_link, *uretprobe_link;
	struct test_attach_probe_manual *skel;
	ssize_t uprobe_offset;

	skel = test_attach_probe_manual__open_and_load();
	if (!ASSERT_OK_PTR(skel, "skel_kprobe_manual_open_and_load"))
		return;

	uprobe_offset = get_uprobe_offset(&trigger_func);
	if (!ASSERT_GE(uprobe_offset, 0, "uprobe_offset"))
		goto cleanup;

	 
	kprobe_opts.attach_mode = attach_mode;
	kprobe_opts.retprobe = false;
	kprobe_link = bpf_program__attach_kprobe_opts(skel->progs.handle_kprobe,
						      SYS_NANOSLEEP_KPROBE_NAME,
						      &kprobe_opts);
	if (!ASSERT_OK_PTR(kprobe_link, "attach_kprobe"))
		goto cleanup;
	skel->links.handle_kprobe = kprobe_link;

	kprobe_opts.retprobe = true;
	kretprobe_link = bpf_program__attach_kprobe_opts(skel->progs.handle_kretprobe,
							 SYS_NANOSLEEP_KPROBE_NAME,
							 &kprobe_opts);
	if (!ASSERT_OK_PTR(kretprobe_link, "attach_kretprobe"))
		goto cleanup;
	skel->links.handle_kretprobe = kretprobe_link;

	 
	uprobe_opts.attach_mode = attach_mode;
	uprobe_opts.ref_ctr_offset = 0;
	uprobe_opts.retprobe = false;
	uprobe_link = bpf_program__attach_uprobe_opts(skel->progs.handle_uprobe,
						      0  ,
						      "/proc/self/exe",
						      uprobe_offset,
						      &uprobe_opts);
	if (!ASSERT_OK_PTR(uprobe_link, "attach_uprobe"))
		goto cleanup;
	skel->links.handle_uprobe = uprobe_link;

	uprobe_opts.retprobe = true;
	uretprobe_link = bpf_program__attach_uprobe_opts(skel->progs.handle_uretprobe,
							 -1  ,
							 "/proc/self/exe",
							 uprobe_offset, &uprobe_opts);
	if (!ASSERT_OK_PTR(uretprobe_link, "attach_uretprobe"))
		goto cleanup;
	skel->links.handle_uretprobe = uretprobe_link;

	 
	uprobe_opts.func_name = "trigger_func2";
	uprobe_opts.retprobe = false;
	uprobe_opts.ref_ctr_offset = 0;
	skel->links.handle_uprobe_byname =
			bpf_program__attach_uprobe_opts(skel->progs.handle_uprobe_byname,
							0  ,
							"/proc/self/exe",
							0, &uprobe_opts);
	if (!ASSERT_OK_PTR(skel->links.handle_uprobe_byname, "attach_uprobe_byname"))
		goto cleanup;

	 
	usleep(1);

	 
	trigger_func();

	 
	trigger_func2();

	ASSERT_EQ(skel->bss->kprobe_res, 1, "check_kprobe_res");
	ASSERT_EQ(skel->bss->kretprobe_res, 2, "check_kretprobe_res");
	ASSERT_EQ(skel->bss->uprobe_res, 3, "check_uprobe_res");
	ASSERT_EQ(skel->bss->uretprobe_res, 4, "check_uretprobe_res");
	ASSERT_EQ(skel->bss->uprobe_byname_res, 5, "check_uprobe_byname_res");

cleanup:
	test_attach_probe_manual__destroy(skel);
}

static void test_attach_probe_auto(struct test_attach_probe *skel)
{
	struct bpf_link *uprobe_err_link;

	 
	skel->links.handle_kprobe_auto = bpf_program__attach(skel->progs.handle_kprobe_auto);
	ASSERT_OK_PTR(skel->links.handle_kprobe_auto, "attach_kprobe_auto");

	skel->links.handle_kretprobe_auto = bpf_program__attach(skel->progs.handle_kretprobe_auto);
	ASSERT_OK_PTR(skel->links.handle_kretprobe_auto, "attach_kretprobe_auto");

	 
	uprobe_err_link = bpf_program__attach(skel->progs.handle_uprobe_byname);
	if (!ASSERT_EQ(libbpf_get_error(uprobe_err_link), -EOPNOTSUPP,
		       "auto-attach should fail for old-style name"))
		return;

	 
	skel->links.handle_uretprobe_byname =
			bpf_program__attach(skel->progs.handle_uretprobe_byname);
	if (!ASSERT_OK_PTR(skel->links.handle_uretprobe_byname, "attach_uretprobe_byname"))
		return;

	 
	usleep(1);

	 
	trigger_func2();

	ASSERT_EQ(skel->bss->kprobe2_res, 11, "check_kprobe_auto_res");
	ASSERT_EQ(skel->bss->kretprobe2_res, 22, "check_kretprobe_auto_res");
	ASSERT_EQ(skel->bss->uretprobe_byname_res, 6, "check_uretprobe_byname_res");
}

static void test_uprobe_lib(struct test_attach_probe *skel)
{
	DECLARE_LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts);
	FILE *devnull;

	 
	uprobe_opts.func_name = "fopen";
	uprobe_opts.retprobe = false;
	skel->links.handle_uprobe_byname2 =
			bpf_program__attach_uprobe_opts(skel->progs.handle_uprobe_byname2,
							0  ,
							"libc.so.6",
							0, &uprobe_opts);
	if (!ASSERT_OK_PTR(skel->links.handle_uprobe_byname2, "attach_uprobe_byname2"))
		return;

	uprobe_opts.func_name = "fclose";
	uprobe_opts.retprobe = true;
	skel->links.handle_uretprobe_byname2 =
			bpf_program__attach_uprobe_opts(skel->progs.handle_uretprobe_byname2,
							-1  ,
							"libc.so.6",
							0, &uprobe_opts);
	if (!ASSERT_OK_PTR(skel->links.handle_uretprobe_byname2, "attach_uretprobe_byname2"))
		return;

	 
	devnull = fopen("/dev/null", "r");
	fclose(devnull);

	ASSERT_EQ(skel->bss->uprobe_byname2_res, 7, "check_uprobe_byname2_res");
	ASSERT_EQ(skel->bss->uretprobe_byname2_res, 8, "check_uretprobe_byname2_res");
}

static void test_uprobe_ref_ctr(struct test_attach_probe *skel)
{
	DECLARE_LIBBPF_OPTS(bpf_uprobe_opts, uprobe_opts);
	struct bpf_link *uprobe_link, *uretprobe_link;
	ssize_t uprobe_offset, ref_ctr_offset;

	uprobe_offset = get_uprobe_offset(&trigger_func4);
	if (!ASSERT_GE(uprobe_offset, 0, "uprobe_offset_ref_ctr"))
		return;

	ref_ctr_offset = get_rel_offset((uintptr_t)&uprobe_ref_ctr);
	if (!ASSERT_GE(ref_ctr_offset, 0, "ref_ctr_offset"))
		return;

	ASSERT_EQ(uprobe_ref_ctr, 0, "uprobe_ref_ctr_before");

	uprobe_opts.retprobe = false;
	uprobe_opts.ref_ctr_offset = ref_ctr_offset;
	uprobe_link = bpf_program__attach_uprobe_opts(skel->progs.handle_uprobe_ref_ctr,
						      0  ,
						      "/proc/self/exe",
						      uprobe_offset,
						      &uprobe_opts);
	if (!ASSERT_OK_PTR(uprobe_link, "attach_uprobe_ref_ctr"))
		return;
	skel->links.handle_uprobe_ref_ctr = uprobe_link;

	ASSERT_GT(uprobe_ref_ctr, 0, "uprobe_ref_ctr_after");

	 
	uprobe_opts.retprobe = true;
	uprobe_opts.ref_ctr_offset = ref_ctr_offset;
	uretprobe_link = bpf_program__attach_uprobe_opts(skel->progs.handle_uretprobe_ref_ctr,
							 -1  ,
							 "/proc/self/exe",
							 uprobe_offset, &uprobe_opts);
	if (!ASSERT_OK_PTR(uretprobe_link, "attach_uretprobe_ref_ctr"))
		return;
	skel->links.handle_uretprobe_ref_ctr = uretprobe_link;
}

static void test_kprobe_sleepable(void)
{
	struct test_attach_kprobe_sleepable *skel;

	skel = test_attach_kprobe_sleepable__open();
	if (!ASSERT_OK_PTR(skel, "skel_kprobe_sleepable_open"))
		return;

	 
	if (!ASSERT_OK(bpf_program__set_flags(skel->progs.handle_kprobe_sleepable,
		BPF_F_SLEEPABLE), "kprobe_sleepable_flags"))
		goto cleanup;

	if (!ASSERT_OK(test_attach_kprobe_sleepable__load(skel),
		       "skel_kprobe_sleepable_load"))
		goto cleanup;

	 
	skel->links.handle_kprobe_sleepable = bpf_program__attach(skel->progs.handle_kprobe_sleepable);
	ASSERT_ERR_PTR(skel->links.handle_kprobe_sleepable, "attach_kprobe_sleepable");

cleanup:
	test_attach_kprobe_sleepable__destroy(skel);
}

static void test_uprobe_sleepable(struct test_attach_probe *skel)
{
	 
	skel->links.handle_uprobe_byname3_sleepable = bpf_program__attach(skel->progs.handle_uprobe_byname3_sleepable);
	if (!ASSERT_OK_PTR(skel->links.handle_uprobe_byname3_sleepable, "attach_uprobe_byname3_sleepable"))
		return;

	skel->links.handle_uprobe_byname3 = bpf_program__attach(skel->progs.handle_uprobe_byname3);
	if (!ASSERT_OK_PTR(skel->links.handle_uprobe_byname3, "attach_uprobe_byname3"))
		return;

	skel->links.handle_uretprobe_byname3_sleepable = bpf_program__attach(skel->progs.handle_uretprobe_byname3_sleepable);
	if (!ASSERT_OK_PTR(skel->links.handle_uretprobe_byname3_sleepable, "attach_uretprobe_byname3_sleepable"))
		return;

	skel->links.handle_uretprobe_byname3 = bpf_program__attach(skel->progs.handle_uretprobe_byname3);
	if (!ASSERT_OK_PTR(skel->links.handle_uretprobe_byname3, "attach_uretprobe_byname3"))
		return;

	skel->bss->user_ptr = test_data;

	 
	trigger_func3();

	ASSERT_EQ(skel->bss->uprobe_byname3_sleepable_res, 9, "check_uprobe_byname3_sleepable_res");
	ASSERT_EQ(skel->bss->uprobe_byname3_res, 10, "check_uprobe_byname3_res");
	ASSERT_EQ(skel->bss->uretprobe_byname3_sleepable_res, 11, "check_uretprobe_byname3_sleepable_res");
	ASSERT_EQ(skel->bss->uretprobe_byname3_res, 12, "check_uretprobe_byname3_res");
}

void test_attach_probe(void)
{
	struct test_attach_probe *skel;

	skel = test_attach_probe__open();
	if (!ASSERT_OK_PTR(skel, "skel_open"))
		return;

	if (!ASSERT_OK(test_attach_probe__load(skel), "skel_load"))
		goto cleanup;
	if (!ASSERT_OK_PTR(skel->bss, "check_bss"))
		goto cleanup;

	if (test__start_subtest("manual-default"))
		test_attach_probe_manual(PROBE_ATTACH_MODE_DEFAULT);
	if (test__start_subtest("manual-legacy"))
		test_attach_probe_manual(PROBE_ATTACH_MODE_LEGACY);
	if (test__start_subtest("manual-perf"))
		test_attach_probe_manual(PROBE_ATTACH_MODE_PERF);
	if (test__start_subtest("manual-link"))
		test_attach_probe_manual(PROBE_ATTACH_MODE_LINK);

	if (test__start_subtest("auto"))
		test_attach_probe_auto(skel);
	if (test__start_subtest("kprobe-sleepable"))
		test_kprobe_sleepable();
	if (test__start_subtest("uprobe-lib"))
		test_uprobe_lib(skel);
	if (test__start_subtest("uprobe-sleepable"))
		test_uprobe_sleepable(skel);
	if (test__start_subtest("uprobe-ref_ctr"))
		test_uprobe_ref_ctr(skel);

cleanup:
	test_attach_probe__destroy(skel);
	ASSERT_EQ(uprobe_ref_ctr, 0, "uprobe_ref_ctr_cleanup");
}
