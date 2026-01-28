#ifndef _LIBSPL_SYS_TYPES_H
#define	_LIBSPL_SYS_TYPES_H
#if defined(HAVE_MAKEDEV_IN_SYSMACROS)
#include <sys/sysmacros.h>
#elif defined(HAVE_MAKEDEV_IN_MKDEV)
#include <sys/mkdev.h>
#endif
#include <sys/isa_defs.h>
#include <sys/feature_tests.h>
#include_next <sys/types.h>
#include <sys/types32.h>
#include <stdarg.h>
#include <sys/stdtypes.h>
#ifndef HAVE_INTTYPES
#include <inttypes.h>
#endif  
typedef uint_t		zoneid_t;
typedef int		projid_t;
#include <sys/param.h>  
#endif
