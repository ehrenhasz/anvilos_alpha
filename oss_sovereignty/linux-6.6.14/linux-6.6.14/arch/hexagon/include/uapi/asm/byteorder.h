#ifndef _ASM_BYTEORDER_H
#define _ASM_BYTEORDER_H
#if defined(__GNUC__) && !defined(__STRICT_ANSI__) || defined(__KERNEL__)
#  define __BYTEORDER_HAS_U64__
#endif
#include <linux/byteorder/little_endian.h>
#endif  
