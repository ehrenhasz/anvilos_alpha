
 
#define pr_fmt(fmt) "rodata_test: " fmt

#include <linux/rodata_test.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <asm/sections.h>

static const int rodata_test_data = 0xC3;

void rodata_test(void)
{
	int zero = 0;

	 
	 
	if (!rodata_test_data) {
		pr_err("test 1 fails (start data)\n");
		return;
	}

	 
	if (!copy_to_kernel_nofault((void *)&rodata_test_data,
				(void *)&zero, sizeof(zero))) {
		pr_err("test data was not read only\n");
		return;
	}

	 
	if (rodata_test_data == zero) {
		pr_err("test data was changed\n");
		return;
	}

	 
	if (!PAGE_ALIGNED(__start_rodata)) {
		pr_err("start of .rodata is not page size aligned\n");
		return;
	}
	if (!PAGE_ALIGNED(__end_rodata)) {
		pr_err("end of .rodata is not page size aligned\n");
		return;
	}

	pr_info("all tests were successful\n");
}
