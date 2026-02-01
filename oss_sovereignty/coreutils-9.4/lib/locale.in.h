 

#@INCLUDE_NEXT@ @NEXT_LOCALE_H@

#else
 

#ifndef _@GUARD_PREFIX@_LOCALE_H

#define _GL_ALREADY_INCLUDING_LOCALE_H

 
#@INCLUDE_NEXT@ @NEXT_LOCALE_H@

#undef _GL_ALREADY_INCLUDING_LOCALE_H

#ifndef _@GUARD_PREFIX@_LOCALE_H
#define _@GUARD_PREFIX@_LOCALE_H

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

 
#include <stddef.h>

 
#if @HAVE_XLOCALE_H@
# include <xlocale.h>
#endif

 

 

 

 
#if !defined LC_MESSAGES
# define LC_MESSAGES 1729
#endif

 
#if defined _MSC_VER
# define int_p_cs_precedes   p_cs_precedes
# define int_p_sign_posn     p_sign_posn
# define int_p_sep_by_space  p_sep_by_space
# define int_n_cs_precedes   n_cs_precedes
# define int_n_sign_posn     n_sign_posn
# define int_n_sep_by_space  n_sep_by_space
#endif

 
#if @REPLACE_STRUCT_LCONV@
# define lconv rpl_lconv
struct lconv
{
   

   
  char *decimal_point;
   
  char *thousands_sep;
   
  char *grouping;

   
  char *mon_decimal_point;
   
  char *mon_thousands_sep;
   
  char *mon_grouping;
   
  char *positive_sign;
   
  char *negative_sign;

   
   
  char *currency_symbol;
   
  char frac_digits;
   
  char p_cs_precedes;
   
  char p_sign_posn;
   
  char p_sep_by_space;
   
  char n_cs_precedes;
   
  char n_sign_posn;
   
  char n_sep_by_space;

   
   
  char *int_curr_symbol;
   
  char int_frac_digits;
   
  char int_p_cs_precedes;
   
  char int_p_sign_posn;
   
  char int_p_sep_by_space;
   
  char int_n_cs_precedes;
   
  char int_n_sign_posn;
   
  char int_n_sep_by_space;
};
#endif

#if @GNULIB_LOCALECONV@
# if @REPLACE_LOCALECONV@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef localeconv
#   define localeconv rpl_localeconv
#  endif
_GL_FUNCDECL_RPL (localeconv, struct lconv *, (void));
_GL_CXXALIAS_RPL (localeconv, struct lconv *, (void));
# else
_GL_CXXALIAS_SYS (localeconv, struct lconv *, (void));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (localeconv);
# endif
#elif @REPLACE_STRUCT_LCONV@
# undef localeconv
# define localeconv localeconv_used_without_requesting_gnulib_module_localeconv
#elif defined GNULIB_POSIXCHECK
# undef localeconv
# if HAVE_RAW_DECL_LOCALECONV
_GL_WARN_ON_USE (localeconv,
                 "localeconv returns too few information on some platforms - "
                 "use gnulib module localeconv for portability");
# endif
#endif

#if @GNULIB_SETLOCALE@
# if @REPLACE_SETLOCALE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef setlocale
#   define setlocale rpl_setlocale
#   define GNULIB_defined_setlocale 1
#  endif
_GL_FUNCDECL_RPL (setlocale, char *, (int category, const char *locale));
_GL_CXXALIAS_RPL (setlocale, char *, (int category, const char *locale));
# else
_GL_CXXALIAS_SYS (setlocale, char *, (int category, const char *locale));
# endif
# if __GLIBC__ >= 2
_GL_CXXALIASWARN (setlocale);
# endif
#elif defined GNULIB_POSIXCHECK
# undef setlocale
# if HAVE_RAW_DECL_SETLOCALE
_GL_WARN_ON_USE (setlocale, "setlocale works differently on native Windows - "
                 "use gnulib module setlocale for portability");
# endif
#endif

#if @GNULIB_SETLOCALE_NULL@
 
# include "setlocale_null.h"
#endif

#if   (@GNULIB_LOCALENAME@ && @LOCALENAME_ENHANCE_LOCALE_FUNCS@ && @HAVE_NEWLOCALE@)
# if @REPLACE_NEWLOCALE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef newlocale
#   define newlocale rpl_newlocale
#   define GNULIB_defined_newlocale 1
#  endif
_GL_FUNCDECL_RPL (newlocale, locale_t,
                  (int category_mask, const char *name, locale_t base)
                  _GL_ARG_NONNULL ((2)));
_GL_CXXALIAS_RPL (newlocale, locale_t,
                  (int category_mask, const char *name, locale_t base));
# else
#  if @HAVE_NEWLOCALE@
_GL_CXXALIAS_SYS (newlocale, locale_t,
                  (int category_mask, const char *name, locale_t base));
#  endif
# endif
# if __GLIBC__ >= 2 && @HAVE_NEWLOCALE@
_GL_CXXALIASWARN (newlocale);
# endif
# if @HAVE_NEWLOCALE@ || @REPLACE_NEWLOCALE@
#  ifndef HAVE_WORKING_NEWLOCALE
#   define HAVE_WORKING_NEWLOCALE 1
#  endif
# endif
#elif defined GNULIB_POSIXCHECK
# undef newlocale
# if HAVE_RAW_DECL_NEWLOCALE
_GL_WARN_ON_USE (newlocale, "newlocale is not portable");
# endif
#endif

#if @GNULIB_DUPLOCALE@ || (@GNULIB_LOCALENAME@ && @LOCALENAME_ENHANCE_LOCALE_FUNCS@ && @HAVE_DUPLOCALE@)
# if @HAVE_DUPLOCALE@  
#  if @REPLACE_DUPLOCALE@
#   if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#    undef duplocale
#    define duplocale rpl_duplocale
#    define GNULIB_defined_duplocale 1
#   endif
_GL_FUNCDECL_RPL (duplocale, locale_t, (locale_t locale) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (duplocale, locale_t, (locale_t locale));
#  else
_GL_CXXALIAS_SYS (duplocale, locale_t, (locale_t locale));
#  endif
# endif
# if __GLIBC__ >= 2 && @HAVE_DUPLOCALE@
_GL_CXXALIASWARN (duplocale);
# endif
# if @HAVE_DUPLOCALE@
#  ifndef HAVE_WORKING_DUPLOCALE
#   define HAVE_WORKING_DUPLOCALE 1
#  endif
# endif
#elif defined GNULIB_POSIXCHECK
# undef duplocale
# if HAVE_RAW_DECL_DUPLOCALE
_GL_WARN_ON_USE (duplocale, "duplocale is buggy on some glibc systems - "
                 "use gnulib module duplocale for portability");
# endif
#endif

#if   (@GNULIB_LOCALENAME@ && @LOCALENAME_ENHANCE_LOCALE_FUNCS@ && @HAVE_FREELOCALE@)
# if @REPLACE_FREELOCALE@
#  if !(defined __cplusplus && defined GNULIB_NAMESPACE)
#   undef freelocale
#   define freelocale rpl_freelocale
#   define GNULIB_defined_freelocale 1
#  endif
_GL_FUNCDECL_RPL (freelocale, void, (locale_t locale) _GL_ARG_NONNULL ((1)));
_GL_CXXALIAS_RPL (freelocale, void, (locale_t locale));
# else
#  if @HAVE_FREELOCALE@
 
_GL_CXXALIAS_SYS_CAST (freelocale, void, (locale_t locale));
#  endif
# endif
# if __GLIBC__ >= 2 && @HAVE_FREELOCALE@
_GL_CXXALIASWARN (freelocale);
# endif
#elif defined GNULIB_POSIXCHECK
# undef freelocale
# if HAVE_RAW_DECL_FREELOCALE
_GL_WARN_ON_USE (freelocale, "freelocale is not portable");
# endif
#endif

#endif  
#endif  
#endif  
