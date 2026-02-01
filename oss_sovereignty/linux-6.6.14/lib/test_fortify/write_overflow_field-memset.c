
#define TEST	\
	memset(instance.buf, 0x42, sizeof(instance.buf) + 1)

#include "test_fortify.h"
