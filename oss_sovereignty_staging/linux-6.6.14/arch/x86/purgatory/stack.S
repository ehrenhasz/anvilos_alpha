 
 

#include <linux/linkage.h>

	 
	.data
	.balign 4096

SYM_DATA_START(stack)
	.skip 4096
SYM_DATA_END_LABEL(stack, SYM_L_GLOBAL, stack_end)
