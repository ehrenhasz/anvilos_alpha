 
#include <uchar.h>

#include <wchar.h>

#if (HAVE_WORKING_MBRTOC32 && !_GL_WCHAR_T_IS_UCS4) || (GL_CHAR32_T_IS_UNICODE && GL_CHAR32_T_VS_WCHAR_T_NEEDS_CONVERSION) || _GL_SMALL_WCHAR_T
 

# include <errno.h>
# include <limits.h>
# include <stdlib.h>

# include "strnlen1.h"

extern mbstate_t _gl_mbsrtoc32s_state;

# define FUNC mbsrtoc32s
# define DCHAR_T char32_t
# define INTERNAL_STATE _gl_mbsrtoc32s_state
# define MBRTOWC mbrtoc32
# if GNULIB_MBRTOC32_REGULAR
    
#  define USES_C32 0
# else
#  define USES_C32 1
# endif
# include "mbsrtowcs-impl.h"

#else
 

static_assert (sizeof (char32_t) == sizeof (wchar_t));

# if _GL_WCHAR_T_IS_UCS4
_GL_EXTERN_INLINE
# endif
size_t
mbsrtoc32s (char32_t *dest, const char **srcp, size_t len, mbstate_t *ps)
{
  return mbsrtowcs ((wchar_t *) dest, srcp, len, ps);
}

#endif
