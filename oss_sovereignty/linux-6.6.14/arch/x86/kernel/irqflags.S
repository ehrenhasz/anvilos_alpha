 

#include <asm/asm.h>
#include <asm/export.h>
#include <linux/linkage.h>

 
.pushsection .noinstr.text, "ax"
SYM_FUNC_START(native_save_fl)
	pushf
	pop %_ASM_AX
	RET
SYM_FUNC_END(native_save_fl)
.popsection
EXPORT_SYMBOL(native_save_fl)
