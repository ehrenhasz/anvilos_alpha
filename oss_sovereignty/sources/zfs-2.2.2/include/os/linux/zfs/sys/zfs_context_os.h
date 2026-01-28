

#ifndef ZFS_CONTEXT_OS_H
#define	ZFS_CONTEXT_OS_H

#include <linux/dcache_compat.h>
#include <linux/utsname_compat.h>
#include <linux/compiler_compat.h>
#include <linux/module.h>

#if THREAD_SIZE >= 16384
#define	HAVE_LARGE_STACKS	1
#endif

#if defined(CONFIG_UML)
#undef setjmp
#undef longjmp
#endif

#endif
