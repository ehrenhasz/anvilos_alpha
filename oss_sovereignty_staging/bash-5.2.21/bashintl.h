 

 

#if !defined (_BASHINTL_H_)
#define _BASHINTL_H_

#if defined (BUILDTOOL)
#  undef ENABLE_NLS
#  define ENABLE_NLS 0
#endif

 
#include "gettext.h"

#if defined (HAVE_LOCALE_H)
#  include <locale.h>
#endif

#define _(msgid)	gettext(msgid)
#define N_(msgid)	msgid
#define D_(d, msgid)	dgettext(d, msgid)

#define P_(m1, m2, n)	ngettext(m1, m2, n)

#if defined (HAVE_SETLOCALE) && !defined (LC_ALL)
#  undef HAVE_SETLOCALE
#endif

#if !defined (HAVE_SETLOCALE)
#  define setlocale(cat, loc)
#endif

#if !defined (HAVE_LOCALE_H) || !defined (HAVE_LOCALECONV)
#  define locale_decpoint()	'.'
#endif

#endif  
