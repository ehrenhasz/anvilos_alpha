 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if HAVE_ISNANL_IN_LIBC
 
# include <math.h>
# if (__GNUC__ >= 4) || (__clang_major__ >= 4)
    
#  undef isnanl
#  define isnanl(x) __builtin_isnan ((long double)(x))
# elif defined isnan
#  undef isnanl
#  define isnanl(x) isnan ((long double)(x))
# endif
#else
 
# undef isnanl
# define isnanl rpl_isnanl
extern int isnanl (long double x);
#endif
