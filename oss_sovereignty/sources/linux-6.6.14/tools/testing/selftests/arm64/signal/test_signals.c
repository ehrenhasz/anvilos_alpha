

#include <kselftest.h>

#include "test_signals.h"
#include "test_signals_utils.h"

struct tdescr *current = &tde;

int main(int argc, char *argv[])
{
	ksft_print_msg("%s :: %s\n", current->name, current->descr);
	if (test_setup(current) && test_init(current)) {
		test_run(current);
		test_cleanup(current);
	}
	test_result(current);

	return current->result;
}
