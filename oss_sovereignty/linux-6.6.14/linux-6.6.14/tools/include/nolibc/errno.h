#ifndef _NOLIBC_ERRNO_H
#define _NOLIBC_ERRNO_H
#include <asm/errno.h>
#ifndef NOLIBC_IGNORE_ERRNO
#define SET_ERRNO(v) do { errno = (v); } while (0)
int errno __attribute__((weak));
#else
#define SET_ERRNO(v) do { } while (0)
#endif
#define MAX_ERRNO 4095
#include "nolibc.h"
#endif  
