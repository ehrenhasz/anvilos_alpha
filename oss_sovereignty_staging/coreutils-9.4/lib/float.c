 
#include <float.h>

#if (defined _ARCH_PPC || defined _POWER) && (defined _AIX || defined __linux__) && (LDBL_MANT_DIG == 106) && defined __GNUC__
const union gl_long_double_union gl_LDBL_MAX =
  { { DBL_MAX, DBL_MAX / (double)134217728UL / (double)134217728UL } };
#elif defined __i386__
const union gl_long_double_union gl_LDBL_MAX =
  { { 0xFFFFFFFF, 0xFFFFFFFF, 32766 } };
#else
 
typedef int dummy;
#endif
