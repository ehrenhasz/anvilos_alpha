
#ifndef MICROPY_INCLUDED_WINDOWS_SLEEP_H
#define MICROPY_INCLUDED_WINDOWS_SLEEP_H


void msec_sleep(double msec);




#ifdef _MSC_VER
int usleep(__int64 usec);
#endif

#endif 
