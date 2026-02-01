 
#ifndef _TOOLS_LINUX_ASM_GENERIC_BITOPS_ATOMIC_H_
#define _TOOLS_LINUX_ASM_GENERIC_BITOPS_ATOMIC_H_

#include <asm/types.h>
#include <asm/bitsperlong.h>

 
#define set_bit test_and_set_bit
#define clear_bit test_and_clear_bit

#endif  
