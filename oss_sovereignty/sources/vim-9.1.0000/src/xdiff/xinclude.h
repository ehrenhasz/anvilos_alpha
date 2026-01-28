




#ifdef HAVE_CONFIG_H
# ifdef VMS
#  include "[.auto]config.h"
# else
#  include "../auto/config.h"
# endif
#endif



#if defined(HAVE_ATTRIBUTE_UNUSED) || defined(__MINGW32__)
# define UNUSED __attribute__((unused))
#else
# define UNUSED
#endif

#if defined(_MSC_VER)
# define inline __inline
#endif

#if !defined(XINCLUDE_H)
#define XINCLUDE_H


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <string.h>
#include <limits.h>

#if 0
#include "git-compat-util.h"
#endif
#include "xmacros.h"
#include "xdiff.h"
#include "xtypes.h"
#include "xutils.h"
#include "xprepare.h"
#include "xdiffi.h"
#include "xemit.h"


#endif 
