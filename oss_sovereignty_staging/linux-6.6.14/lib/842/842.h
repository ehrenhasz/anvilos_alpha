 

#ifndef __842_H__
#define __842_H__

 

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/crc32.h>
#include <asm/unaligned.h>

#include <linux/sw842.h>

 
#define OP_REPEAT	(0x1B)
#define OP_ZEROS	(0x1C)
#define OP_END		(0x1E)

 
#define OP_SHORT_DATA	(0x1D)

 
#define OP_BITS		(5)
#define REPEAT_BITS	(6)
#define SHORT_DATA_BITS	(3)
#define I2_BITS		(8)
#define I4_BITS		(9)
#define I8_BITS		(8)
#define CRC_BITS	(32)

#define REPEAT_BITS_MAX		(0x3f)
#define SHORT_DATA_BITS_MAX	(0x7)

 
#define OP_ACTION	(0x70)
#define OP_ACTION_INDEX	(0x10)
#define OP_ACTION_DATA	(0x20)
#define OP_ACTION_NOOP	(0x40)
#define OP_AMOUNT	(0x0f)
#define OP_AMOUNT_0	(0x00)
#define OP_AMOUNT_2	(0x02)
#define OP_AMOUNT_4	(0x04)
#define OP_AMOUNT_8	(0x08)

#define D2		(OP_ACTION_DATA  | OP_AMOUNT_2)
#define D4		(OP_ACTION_DATA  | OP_AMOUNT_4)
#define D8		(OP_ACTION_DATA  | OP_AMOUNT_8)
#define I2		(OP_ACTION_INDEX | OP_AMOUNT_2)
#define I4		(OP_ACTION_INDEX | OP_AMOUNT_4)
#define I8		(OP_ACTION_INDEX | OP_AMOUNT_8)
#define N0		(OP_ACTION_NOOP  | OP_AMOUNT_0)

 
#define OPS_MAX		(0x1a)

#endif
