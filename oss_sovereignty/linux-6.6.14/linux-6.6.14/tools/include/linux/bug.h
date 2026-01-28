#ifndef _TOOLS_PERF_LINUX_BUG_H
#define _TOOLS_PERF_LINUX_BUG_H
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))
#endif	 
