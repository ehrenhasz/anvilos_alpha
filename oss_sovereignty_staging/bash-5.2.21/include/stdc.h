 

 

#if !defined (_STDC_H_)
#define _STDC_H_

 

 

#if !defined (PARAMS)
#  if defined (__STDC__) || defined (__GNUC__) || defined (__cplusplus) || defined (PROTOTYPES)
#    define PARAMS(protos) protos
#  else 
#    define PARAMS(protos) ()
#  endif
#endif

 
#if defined (HAVE_STRINGIZE)
#  define CPP_STRING(x) #x
#else
#  define CPP_STRING(x) "x"
#endif

#if !defined (__STDC__)

#if defined (__GNUC__)		 
#  if !defined (signed)
#    define signed __signed
#  endif
#  if !defined (volatile)
#    define volatile __volatile
#  endif
#else  
#  if !defined (inline)
#    define inline
#  endif
#  if !defined (signed)
#    define signed
#  endif
#  if !defined (volatile)
#    define volatile
#  endif
#endif  

#endif  

#ifndef __attribute__
#  if __GNUC__ < 2 || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#    define __attribute__(x)
#  endif
#endif

 
#ifdef __GNUC__
#  define INLINE inline
#else
#  define INLINE
#endif

#if defined (PREFER_STDARG)
#  define SH_VA_START(va, arg)  va_start(va, arg)
#else
#  define SH_VA_START(va, arg)  va_start(va)
#endif

#endif  
