#ifndef _ORC_TYPES_H
#define _ORC_TYPES_H
#include <linux/types.h>
#include <linux/compiler.h>
#define ORC_REG_UNDEFINED		0
#define ORC_REG_PREV_SP			1
#define ORC_REG_DX			2
#define ORC_REG_DI			3
#define ORC_REG_BP			4
#define ORC_REG_SP			5
#define ORC_REG_R10			6
#define ORC_REG_R13			7
#define ORC_REG_BP_INDIRECT		8
#define ORC_REG_SP_INDIRECT		9
#define ORC_REG_MAX			15
#define ORC_TYPE_UNDEFINED		0
#define ORC_TYPE_END_OF_STACK		1
#define ORC_TYPE_CALL			2
#define ORC_TYPE_REGS			3
#define ORC_TYPE_REGS_PARTIAL		4
#ifndef __ASSEMBLY__
#include <asm/byteorder.h>
struct orc_entry {
	s16		sp_offset;
	s16		bp_offset;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned	sp_reg:4;
	unsigned	bp_reg:4;
	unsigned	type:3;
	unsigned	signal:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	unsigned	bp_reg:4;
	unsigned	sp_reg:4;
	unsigned	unused:4;
	unsigned	signal:1;
	unsigned	type:3;
#endif
} __packed;
#endif  
#endif  
