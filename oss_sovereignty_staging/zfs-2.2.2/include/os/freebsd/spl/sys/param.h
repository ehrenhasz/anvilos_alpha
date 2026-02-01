 

#ifndef _COMPAT_OPENSOLARIS_SYS_PARAM_H_
#define	_COMPAT_OPENSOLARIS_SYS_PARAM_H_

#include <sys/types.h>
#include_next <sys/param.h>
#define	PAGESIZE	PAGE_SIZE
#define	ptob(x)		((uint64_t)(x) << PAGE_SHIFT)
#ifdef _KERNEL
#include <sys/systm.h>
#include <sys/libkern.h>
#endif
#endif
