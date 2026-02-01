 

 

#ifndef _@GUARD_PREFIX@_CTYPE_H

#if __GNUC__ >= 3
@PRAGMA_SYSTEM_HEADER@
#endif
@PRAGMA_COLUMNS@

 
 
#@INCLUDE_NEXT@ @NEXT_CTYPE_H@

#ifndef _@GUARD_PREFIX@_CTYPE_H
#define _@GUARD_PREFIX@_CTYPE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 

 

 
#if @GNULIB_ISBLANK@
# if !@HAVE_ISBLANK@
_GL_EXTERN_C int isblank (int c);
# endif
#elif defined GNULIB_POSIXCHECK
# undef isblank
# if HAVE_RAW_DECL_ISBLANK
_GL_WARN_ON_USE (isblank, "isblank is unportable - "
                 "use gnulib module isblank for portability");
# endif
#endif

#endif  
#endif  
