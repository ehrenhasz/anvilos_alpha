#ifndef _ORC_HEADER_H
#define _ORC_HEADER_H
#include <linux/types.h>
#include <linux/compiler.h>
#include <asm/orc_hash.h>
#define ORC_HEADER					\
	__used __section(".orc_header") __aligned(4)	\
	static const u8 orc_header[] = { ORC_HASH }
#endif  
