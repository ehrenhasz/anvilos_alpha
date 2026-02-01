 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#if HAVE_ISNANF_IN_LIBC
 
# include <math.h>
# if (__GNUC__ >= 4) || (__clang_major__ >= 4)
    
#  undef isnanf
#  define isnanf(x) __builtin_isnan ((float)(x))
# elif defined isnan
#  undef isnanf
#  define isnanf(x) isnan ((float)(x))
# else
    
#  if defined __sgi
    
extern int isnanf (float x);
#  endif
# endif
#else
 
# undef isnanf
# define isnanf rpl_isnanf
extern int isnanf (float x);
#endif
