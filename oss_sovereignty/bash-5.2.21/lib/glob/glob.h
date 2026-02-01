 

#ifndef	_GLOB_H_
#define	_GLOB_H_

#include "stdc.h"

#define GX_MARKDIRS	0x001	 
#define GX_NOCASE	0x002	 
#define GX_MATCHDOT	0x004	 
#define GX_MATCHDIRS	0x008	 
#define GX_ALLDIRS	0x010	 
#define GX_NULLDIR	0x100	 
#define GX_ADDCURDIR	0x200	 
#define GX_GLOBSTAR	0x400	 
#define GX_RECURSE	0x800	 
#define GX_SYMLINK	0x1000	 
#define GX_NEGATE	0x2000	 

extern int glob_pattern_p PARAMS((const char *));
extern char **glob_vector PARAMS((char *, char *, int));
extern char **glob_filename PARAMS((char *, int));

extern int extglob_pattern_p PARAMS((const char *));

extern char *glob_error_return;
extern int noglob_dot_filenames;
extern int glob_ignore_case;

#endif  
