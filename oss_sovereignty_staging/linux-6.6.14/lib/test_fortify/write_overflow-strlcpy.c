
#define TEST	\
	strlcpy(instance.buf, large_src, sizeof(instance.buf) + 1)

#include "test_fortify.h"
