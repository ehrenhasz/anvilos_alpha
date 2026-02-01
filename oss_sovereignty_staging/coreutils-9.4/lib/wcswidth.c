 
#include <wchar.h>

#include <limits.h>

#define FUNC wcswidth
#define UNIT wchar_t
#define CHARACTER_WIDTH wcwidth
#include "wcswidth-impl.h"
