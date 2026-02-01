
 
#include <test_progs.h>
#include "timer.skel.h"
#include "timer_failure.skel.h"

static int timer(struct timer *timer_skel)
{
	int err, prog_fd;
	LIBBPF_OPTS(bpf_test_run_opts, topts);

	err = timer__attach(timer_skel);
	if (!ASSERT_OK(err, "timer_attach"))
		return err;

	ASSERT_EQ(timer_skel->data->callback_check, 52, "callback_check1");
	ASSERT_EQ(timer_skel->data->callback2_check, 52, "callback2_check1");

	prog_fd = bpf_program__fd(timer_skel->progs.test1);
	err = bpf_prog_test_run_opts(prog_fd, &topts);
	ASSERT_OK(err, "test_run");
	ASSERT_EQ(topts.retval, 0, "test_run");
	timer__detach(timer_skel);

	usleep(50);  
	 
	ASSERT_EQ(timer_skel->data->callback_check, 42, "callback_check2");
	ASSERT_EQ(timer_skel->data->callback2_check, 42, "callback2_check2");

	 
	ASSERT_EQ(timer_skel->bss->bss_data, 10, "bss_data");

	 
	ASSERT_EQ(timer_skel->bss->abs_data, 12, "abs_data");

	 
	ASSERT_EQ(timer_skel->bss->err, 0, "err");

	 
	ASSERT_EQ(timer_skel->bss->ok, 1 | 2 | 4, "ok");

	return 0;
}

 
void serial_test_timer(void)
{
	struct timer *timer_skel = NULL;
	int err;

	timer_skel = timer__open_and_load();
	if (!ASSERT_OK_PTR(timer_skel, "timer_skel_load"))
		return;

	err = timer(timer_skel);
	ASSERT_OK(err, "timer");
	timer__destroy(timer_skel);

	RUN_TESTS(timer_failure);
}
