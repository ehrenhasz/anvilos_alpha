#ifndef _RTL871X_BYTEORDER_H_
#define _RTL871X_BYTEORDER_H_
#if defined(__LITTLE_ENDIAN)
#include <linux/byteorder/little_endian.h>
#else
#  include <linux/byteorder/big_endian.h>
#endif
#endif  
