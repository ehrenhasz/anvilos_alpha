
#include "mpconfigport.h"

#ifdef TEST
#include "shared/upytesthelper/upytesthelper.h"
#define MP_PLAT_PRINT_STRN(str, len) upytest_output(str, len)
#endif
