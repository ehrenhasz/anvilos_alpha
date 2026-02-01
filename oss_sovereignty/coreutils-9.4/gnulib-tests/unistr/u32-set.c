 
#include "unistr.h"

#define FUNC u32_set
#define UNIT uint32_t
#define IS_SINGLE_UNIT(uc) (uc < 0xd800 || (uc >= 0xe000 && uc < 0x110000))
#include "u-set.h"
