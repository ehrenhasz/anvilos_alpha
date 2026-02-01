
 

#include <test_progs.h>
#include <time.h>
#include "test_autoload.skel.h"

void test_autoload(void)
{
	int duration = 0, err;
	struct test_autoload* skel;

	skel = test_autoload__open_and_load();
	 
	if (CHECK(skel, "skel_open_and_load", "unexpected success\n"))
		goto cleanup;

	skel = test_autoload__open();
	if (CHECK(!skel, "skel_open", "failed to open skeleton\n"))
		goto cleanup;

	 
	bpf_program__set_autoload(skel->progs.prog3, false);

	err = test_autoload__load(skel);
	if (CHECK(err, "skel_load", "failed to load skeleton: %d\n", err))
		goto cleanup;

	err = test_autoload__attach(skel);
	if (CHECK(err, "skel_attach", "skeleton attach failed: %d\n", err))
		goto cleanup;

	usleep(1);

	CHECK(!skel->bss->prog1_called, "prog1", "not called\n");
	CHECK(!skel->bss->prog2_called, "prog2", "not called\n");
	CHECK(skel->bss->prog3_called, "prog3", "called?!\n");

cleanup:
	test_autoload__destroy(skel);
}
