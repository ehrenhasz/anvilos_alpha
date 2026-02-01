
 

#include <test_progs.h>
#include "test_access_variable_array.skel.h"

void test_access_variable_array(void)
{
	struct test_access_variable_array *skel;

	skel = test_access_variable_array__open_and_load();
	if (!ASSERT_OK_PTR(skel, "test_access_variable_array__open_and_load"))
		return;

	test_access_variable_array__destroy(skel);
}
