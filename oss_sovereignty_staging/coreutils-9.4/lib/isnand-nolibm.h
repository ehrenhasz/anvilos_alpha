 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if HAVE_ISNAND_IN_LIBC
 
# include <math.h>
# if (__GNUC__ >= 4) || (__clang_major__ >= 4)
    
#  undef isnand
#  define isnand(x) __builtin_isnan ((double)(x))
# else
#  undef isnand
#  define isnand(x) isnan ((double)(x))
# endif
#else
 
# undef isnand
# define isnand rpl_isnand
extern int isnand (double x);
#endif
