#ifndef	ZED_EXEC_H
#define	ZED_EXEC_H
#include <stdint.h>
#include "zed_strings.h"
#include "zed_conf.h"
void zed_exec_fini(void);
int zed_exec_process(uint64_t eid, const char *class, const char *subclass,
    struct zed_conf *zcp, zed_strings_t *envs);
#endif	 
