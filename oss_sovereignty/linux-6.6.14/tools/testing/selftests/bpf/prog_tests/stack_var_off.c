
#include <test_progs.h>
#include "test_stack_var_off.skel.h"

 
void test_stack_var_off(void)
{
	int duration = 0;
	struct test_stack_var_off *skel;

	skel = test_stack_var_off__open_and_load();
	if (CHECK(!skel, "skel_open", "failed to open skeleton\n"))
		return;

	 
	skel->bss->test_pid = getpid();
	 
	skel->bss->input[0] = 2;
	skel->bss->input[1] = 42;   

	if (!ASSERT_OK(test_stack_var_off__attach(skel), "skel_attach"))
		goto cleanup;

	 
	usleep(1);

	if (CHECK(skel->bss->probe_res != 42, "check_probe_res",
		  "wrong probe res: %d\n", skel->bss->probe_res))
		goto cleanup;

cleanup:
	test_stack_var_off__destroy(skel);
}
