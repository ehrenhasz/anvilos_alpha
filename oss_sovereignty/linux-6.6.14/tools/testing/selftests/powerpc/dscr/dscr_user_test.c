
 
#include "dscr.h"

static int check_dscr(char *str)
{
	unsigned long cur_dscr, cur_dscr_usr;

	cur_dscr = get_dscr();
	cur_dscr_usr = get_dscr_usr();
	if (cur_dscr != cur_dscr_usr) {
		printf("%s set, kernel get %lx != user get %lx\n",
					str, cur_dscr, cur_dscr_usr);
		return 1;
	}
	return 0;
}

int dscr_user(void)
{
	int i;

	SKIP_IF(!have_hwcap2(PPC_FEATURE2_DSCR));

	check_dscr("");

	for (i = 0; i < COUNT; i++) {
		set_dscr(i);
		if (check_dscr("kernel"))
			return 1;
	}

	for (i = 0; i < COUNT; i++) {
		set_dscr_usr(i);
		if (check_dscr("user"))
			return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	return test_harness(dscr_user, "dscr_user_test");
}
