

#include <asm/bootparam.h>

	.section ".rodata.kernel_info", "a"

	.global kernel_info

kernel_info:
	
	.ascii	"LToP"
	
	.long	kernel_info_var_len_data - kernel_info
	
	.long	kernel_info_end - kernel_info

	
	.long	SETUP_TYPE_MAX

kernel_info_var_len_data:
	
kernel_info_end:
