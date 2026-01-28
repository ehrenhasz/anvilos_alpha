#ifndef _NOLIBC_STD_H
#define _NOLIBC_STD_H
#ifndef NULL
#define NULL ((void *)0)
#endif
#include "stdint.h"
typedef unsigned int          dev_t;
typedef unsigned long         ino_t;
typedef unsigned int         mode_t;
typedef   signed int          pid_t;
typedef unsigned int          uid_t;
typedef unsigned int          gid_t;
typedef unsigned long       nlink_t;
typedef   signed long         off_t;
typedef   signed long     blksize_t;
typedef   signed long      blkcnt_t;
typedef   signed long        time_t;
#endif  
