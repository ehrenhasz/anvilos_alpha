

#include <stdint.h>



typedef intptr_t mp_int_t; 
typedef uintptr_t mp_uint_t; 
typedef long mp_off_t;


#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <stdlib.h>
#else
#include <alloca.h>
#endif

#define MICROPY_MPHALPORT_H "port/mphalport.h"
