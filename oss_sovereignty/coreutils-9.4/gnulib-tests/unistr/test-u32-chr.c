 

#include <config.h>

#include "unistr.h"

#include <stdlib.h>
#include <string.h>

#include "zerosize-ptr.h"
#include "macros.h"

#define UNIT uint32_t
#define U_UCTOMB(s, uc, n) (*(s) = (uc), 1)
#define U32_TO_U(s, n, result, length) (*(length) = (n), (s))
#define U_CHR u32_chr
#define U_SET u32_set
#include "test-chr.h"
