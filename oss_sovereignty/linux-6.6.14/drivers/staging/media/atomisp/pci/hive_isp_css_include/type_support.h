 
 

#ifndef __TYPE_SUPPORT_H_INCLUDED__
#define __TYPE_SUPPORT_H_INCLUDED__

 

#define IA_CSS_UINT8_T_BITS						8
#define IA_CSS_UINT16_T_BITS					16
#define IA_CSS_UINT32_T_BITS					32
#define IA_CSS_INT32_T_BITS						32
#define IA_CSS_UINT64_T_BITS					64

#define CHAR_BIT (8)

#include <linux/types.h>
#include <linux/limits.h>
#include <linux/errno.h>
#define HOST_ADDRESS(x) (unsigned long)(x)

#endif  
