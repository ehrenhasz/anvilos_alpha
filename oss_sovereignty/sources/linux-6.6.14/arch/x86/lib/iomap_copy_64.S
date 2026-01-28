


#include <linux/linkage.h>


SYM_FUNC_START(__iowrite32_copy)
	movl %edx,%ecx
	rep movsl
	RET
SYM_FUNC_END(__iowrite32_copy)
