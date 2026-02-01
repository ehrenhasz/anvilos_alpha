 

 

#include <config.h>

#if defined (HAVE_UNISTD_H)
#  ifdef _MINIX
#    include <sys/types.h>
#  endif
#  include <unistd.h>
#endif

#include <bashansi.h>
#include "shell.h"

#include <tilde/tilde.h>

#ifndef NULL
#  define NULL 0
#endif

 

#ifndef MP_DOTILDE
#  define MP_DOTILDE	0x01
#  define MP_DOCWD	0x02
#  define MP_RMDOT	0x04
#  define MP_IGNDOT	0x08
#endif

extern char *get_working_directory PARAMS((char *));

static char *nullpath = "";

 

#define MAKEDOT() \
  do { \
    xpath = (char *)xmalloc (2); \
    xpath[0] = '.'; \
    xpath[1] = '\0'; \
    pathlen = 1; \
  } while (0)

char *
sh_makepath (path, dir, flags)
     const char *path, *dir;
     int flags;
{
  int dirlen, pathlen;
  char *ret, *xpath, *xdir, *r, *s;

  if (path == 0 || *path == '\0')
    {
      if (flags & MP_DOCWD)
	{
	  xpath = get_working_directory ("sh_makepath");
	  if (xpath == 0)
	    {
	      ret = get_string_value ("PWD");
	      if (ret)
		xpath = savestring (ret);
	    }
	  if (xpath == 0)
	    MAKEDOT();
	  else
	    pathlen = strlen (xpath);
	}
      else
	MAKEDOT();
    }
  else if ((flags & MP_IGNDOT) && path[0] == '.' && (path[1] == '\0' ||
						     (path[1] == '/' && path[2] == '\0')))
    {
      xpath = nullpath;
      pathlen = 0;
    }
  else
    {
      xpath = ((flags & MP_DOTILDE) && *path == '~') ? bash_tilde_expand (path, 0) : (char *)path;
      pathlen = strlen (xpath);
    }

  xdir = (char *)dir;
  dirlen = strlen (xdir);
  if ((flags & MP_RMDOT) && dir[0] == '.' && dir[1] == '/')
    {
      xdir += 2;
      dirlen -= 2;
    }

  r = ret = (char *)xmalloc (2 + dirlen + pathlen);
  s = xpath;
  while (*s)
    *r++ = *s++;
  if (s > xpath && s[-1] != '/')
    *r++ = '/';      
  s = xdir;
  while (*r++ = *s++)
    ;
  if (xpath != path && xpath != nullpath)
    free (xpath);
  return (ret);
}
