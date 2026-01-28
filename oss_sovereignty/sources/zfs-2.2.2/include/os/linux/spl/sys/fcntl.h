

#ifndef _SPL_FCNTL_H
#define	_SPL_FCNTL_H

#include <asm/fcntl.h>

#define	F_FREESP 11

#ifdef CONFIG_64BIT
typedef struct flock flock64_t;
#else
typedef struct flock64 flock64_t;
#endif 

#endif 
