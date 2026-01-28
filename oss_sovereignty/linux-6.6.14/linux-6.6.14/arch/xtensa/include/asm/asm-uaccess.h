#ifndef _XTENSA_ASM_UACCESS_H
#define _XTENSA_ASM_UACCESS_H
#include <linux/errno.h>
#include <asm/types.h>
#include <asm/current.h>
#include <asm/asm-offsets.h>
#include <asm/processor.h>
	.macro	user_ok	aa, as, at, error
	movi	\at, __XTENSA_UL_CONST(TASK_SIZE)
	bgeu	\as, \at, \error
	sub	\at, \at, \as
	bgeu	\aa, \at, \error
	.endm
	.macro	access_ok  aa, as, at, sp, error
	user_ok    \aa, \as, \at, \error
.Laccess_ok_\@:
	.endm
#endif	 
