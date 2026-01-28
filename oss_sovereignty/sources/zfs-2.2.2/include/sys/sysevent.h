


#ifndef _SYS_SYSEVENT_H
#define	_SYS_SYSEVENT_H

#include <sys/nvpair.h>

typedef struct sysevent {
	nvlist_t *resource;
} sysevent_t;

#endif
