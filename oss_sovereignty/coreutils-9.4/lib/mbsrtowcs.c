 
#include <wchar.h>

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#include "strnlen1.h"


extern mbstate_t _gl_mbsrtowcs_state;

#define FUNC mbsrtowcs
#define DCHAR_T wchar_t
#define INTERNAL_STATE _gl_mbsrtowcs_state
#define MBRTOWC mbrtowc
#define USES_C32 0
#include "mbsrtowcs-impl.h"
