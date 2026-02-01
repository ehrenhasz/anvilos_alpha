 
#ifndef _LINUX_RESOURCE_H
#define _LINUX_RESOURCE_H

#include <uapi/linux/resource.h>


struct task_struct;

void getrusage(struct task_struct *p, int who, struct rusage *ru);

#endif
