
 

#include <test_progs.h>

 
#include "exhandler_kern.skel.h"

void test_exhandler(void)
{
	int err = 0, duration = 0, status;
	struct exhandler_kern *skel;
	pid_t cpid;

	skel = exhandler_kern__open_and_load();
	if (CHECK(!skel, "skel_load", "skeleton failed: %d\n", err))
		goto cleanup;

	skel->bss->test_pid = getpid();

	err = exhandler_kern__attach(skel);
	if (!ASSERT_OK(err, "attach"))
		goto cleanup;
	cpid = fork();
	if (!ASSERT_GT(cpid, -1, "fork failed"))
		goto cleanup;
	if (cpid == 0)
		_exit(0);
	waitpid(cpid, &status, 0);

	ASSERT_NEQ(skel->bss->exception_triggered, 0, "verify exceptions occurred");
cleanup:
	exhandler_kern__destroy(skel);
}
