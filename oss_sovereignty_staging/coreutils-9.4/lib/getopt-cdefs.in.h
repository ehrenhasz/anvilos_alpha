 

 
#if @HAVE_SYS_CDEFS_H@
# include <sys/cdefs.h>
#endif

#ifndef __BEGIN_DECLS
# ifdef __cplusplus
#  define __BEGIN_DECLS extern "C" {
# else
#  define __BEGIN_DECLS  
# endif
#endif
#ifndef __END_DECLS
# ifdef __cplusplus
#  define __END_DECLS }
# else
#  define __END_DECLS  
# endif
#endif

#ifndef __GNUC_PREREQ
# if defined __GNUC__ && defined __GNUC_VERSION__
# define __GNUC_PREREQ(maj, min) \
        ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
# else
#  define __GNUC_PREREQ(maj, min) 0
# endif
#endif

#ifndef __THROW
# if defined __cplusplus && (__GNUC_PREREQ (2,8) || __clang_major__ >= 4)
#  define __THROW       throw ()
# else
#  define __THROW
# endif
#endif

#endif  
