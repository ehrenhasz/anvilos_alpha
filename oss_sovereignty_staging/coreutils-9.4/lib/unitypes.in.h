 
#include <stdint.h>

 
typedef uint32_t ucs4_t;

 
#ifndef _UC_ATTRIBUTE_CONST
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95) || defined __clang__
#  define _UC_ATTRIBUTE_CONST __attribute__ ((__const__))
# else
#  define _UC_ATTRIBUTE_CONST
# endif
#endif

 
#ifndef _UC_ATTRIBUTE_PURE
# if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96) || defined __clang__
#  define _UC_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _UC_ATTRIBUTE_PURE
# endif
#endif

 
#ifndef _UC_RESTRICT
# if defined __restrict \
     || 2 < __GNUC__ + (95 <= __GNUC_MINOR__) \
     || __clang_major__ >= 3
#  define _UC_RESTRICT __restrict
# elif 199901L <= __STDC_VERSION__ || defined restrict
#  define _UC_RESTRICT restrict
# else
#  define _UC_RESTRICT
# endif
#endif

#endif  
