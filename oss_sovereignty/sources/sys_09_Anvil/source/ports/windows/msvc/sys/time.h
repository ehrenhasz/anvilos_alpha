
#ifndef MICROPY_INCLUDED_WINDOWS_MSVC_SYS_TIME_H
#define MICROPY_INCLUDED_WINDOWS_MSVC_SYS_TIME_H


#include <Winsock2.h>

int gettimeofday(struct timeval *tp, struct timezone *tz);

#endif 
