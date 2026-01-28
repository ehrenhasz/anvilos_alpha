
#ifndef CANONICALIZE_H
#define CANONICALIZE_H

#include "c.h"	
#include "strutils.h"

extern char *canonicalize_path(const char *path);
extern char *canonicalize_path_restricted(const char *path);
extern char *canonicalize_dm_name(const char *ptname);
extern char *__canonicalize_dm_name(const char *prefix, const char *ptname);

extern char *absolute_path(const char *path);

static inline int is_relative_path(const char *path)
{
	if (!path || *path == '/')
		return 0;
	return 1;
}

#endif 
