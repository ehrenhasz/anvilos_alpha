 
#@INCLUDE_NEXT@ @NEXT_ICONV_H@

#ifndef _@GUARD_PREFIX@_ICONV_H
#define _@GUARD_PREFIX@_ICONV_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 

 

 


#if @GNULIB_ICONV@
# if @REPLACE_ICONV_OPEN@
 
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define iconv_open rpl_iconv_open
#  endif
_GL_FUNCDECL_RPL (iconv_open, iconv_t,
                  (const char *tocode, const char *fromcode)
                  _GL_ARG_NONNULL ((1, 2)));
_GL_CXXALIAS_RPL (iconv_open, iconv_t,
                  (const char *tocode, const char *fromcode));
# else
_GL_CXXALIAS_SYS (iconv_open, iconv_t,
                  (const char *tocode, const char *fromcode));
# endif
_GL_CXXALIASWARN (iconv_open);
#elif defined GNULIB_POSIXCHECK
# undef iconv_open
# if HAVE_RAW_DECL_ICONV_OPEN
_GL_WARN_ON_USE (iconv_open, "iconv_open is not working correctly everywhere - "
                 "use gnulib module iconv for portability");
# endif
#endif

#if @REPLACE_ICONV_UTF@
 
# define _ICONV_UTF8_UTF16BE (iconv_t)(-161)
# define _ICONV_UTF8_UTF16LE (iconv_t)(-162)
# define _ICONV_UTF8_UTF32BE (iconv_t)(-163)
# define _ICONV_UTF8_UTF32LE (iconv_t)(-164)
# define _ICONV_UTF16BE_UTF8 (iconv_t)(-165)
# define _ICONV_UTF16LE_UTF8 (iconv_t)(-166)
# define _ICONV_UTF32BE_UTF8 (iconv_t)(-167)
# define _ICONV_UTF32LE_UTF8 (iconv_t)(-168)
#endif

#if @GNULIB_ICONV@
# if @REPLACE_ICONV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define iconv rpl_iconv
#  endif
_GL_FUNCDECL_RPL (iconv, size_t,
                  (iconv_t cd,
                   @ICONV_CONST@ char **restrict inbuf,
                   size_t *restrict inbytesleft,
                   char **restrict outbuf, size_t *restrict outbytesleft));
_GL_CXXALIAS_RPL (iconv, size_t,
                  (iconv_t cd,
                   @ICONV_CONST@ char **restrict inbuf,
                   size_t *restrict inbytesleft,
                   char **restrict outbuf, size_t *restrict outbytesleft));
# else
 
_GL_CXXALIAS_SYS_CAST (iconv, size_t,
                       (iconv_t cd,
                        @ICONV_CONST@ char **restrict inbuf,
                        size_t *restrict inbytesleft,
                        char **restrict outbuf, size_t *restrict outbytesleft));
# endif
_GL_CXXALIASWARN (iconv);
# ifndef ICONV_CONST
#  define ICONV_CONST @ICONV_CONST@
# endif
#elif defined GNULIB_POSIXCHECK
# undef iconv
# if HAVE_RAW_DECL_ICONV
_GL_WARN_ON_USE (iconv, "iconv is not working correctly everywhere - "
                 "use gnulib module iconv for portability");
# endif
#endif

#if @GNULIB_ICONV@
# if @REPLACE_ICONV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   define iconv_close rpl_iconv_close
#  endif
_GL_FUNCDECL_RPL (iconv_close, int, (iconv_t cd));
_GL_CXXALIAS_RPL (iconv_close, int, (iconv_t cd));
# else
_GL_CXXALIAS_SYS (iconv_close, int, (iconv_t cd));
# endif
_GL_CXXALIASWARN (iconv_close);
#endif


#endif  
#endif  
