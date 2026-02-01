 
#ifdef _LIBC
typedef long int mktime_offset_t;
#elif defined TIME_T_IS_SIGNED
typedef time_t mktime_offset_t;
#else
typedef int mktime_offset_t;
#endif

 
#if ! (defined _LIBC && __TIMESIZE != 64)
# undef __time64_t
# define __time64_t time_t
# define __gmtime64_r __gmtime_r
# define __localtime64_r __localtime_r
# define __mktime64 mktime
# define __timegm64 timegm
#endif

#ifndef _LIBC

 

# undef __gmtime_r
# undef __localtime_r
# define __gmtime_r gmtime_r
# define __localtime_r localtime_r

# define __mktime_internal mktime_internal

#endif

 
extern __time64_t __mktime_internal (struct tm *tp,
                                     struct tm *(*func) (__time64_t const *,
                                                         struct tm *),
                                     mktime_offset_t *offset) attribute_hidden;
