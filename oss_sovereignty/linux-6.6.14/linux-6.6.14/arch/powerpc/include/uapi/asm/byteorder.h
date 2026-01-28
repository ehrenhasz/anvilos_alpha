#ifndef _ASM_POWERPC_BYTEORDER_H
#define _ASM_POWERPC_BYTEORDER_H
#ifdef __LITTLE_ENDIAN__
#include <linux/byteorder/little_endian.h>
#else
#include <linux/byteorder/big_endian.h>
#endif
#endif  
