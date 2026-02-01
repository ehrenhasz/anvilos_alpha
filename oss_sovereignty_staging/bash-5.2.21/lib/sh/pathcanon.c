 

 

#include <config.h>

#include <bashtypes.h>
#if defined (HAVE_SYS_PARAM_H)
#  include <sys/param.h>
#endif
#include <posixstat.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <filecntl.h>
#include <bashansi.h>
#include <stdio.h>
#include <chartypes.h>
#include <errno.h>

#include "shell.h"

#if !defined (errno)
extern int errno;
#endif

#if defined (__CYGWIN__)
#include <sys/cygwin.h>

static int
_is_cygdrive (path)
     char *path;
{
  static char user[MAXPATHLEN];
  static char system[MAXPATHLEN];
  static int first_time = 1;

   
  if (path[0] == '/' && path[1] == '/' && !strchr (path + 2, '/'))
    return 1; 
   
  if (first_time)
    {
      char user_flags[MAXPATHLEN];
      char system_flags[MAXPATHLEN];
       
      cygwin_internal (CW_GET_CYGDRIVE_INFO, user, system, user_flags, system_flags);
      first_time = 0;
    }
  return !strcasecmp (path, user) || !strcasecmp (path, system);
}
#endif  	

 
static int
_path_isdir (path)
     char *path;
{
  int l;
  struct stat sb;

   
  errno = 0;
  l = stat (path, &sb) == 0 && S_ISDIR (sb.st_mode);
#if defined (__CYGWIN__)
  if (l == 0)
    l = _is_cygdrive (path);
#endif
  return l;
}

 

 

#define DOUBLE_SLASH(p)	((p[0] == '/') && (p[1] == '/') && p[2] != '/')

char *
sh_canonpath (path, flags)
     char *path;
     int flags;
{
  char stub_char;
  char *result, *p, *q, *base, *dotdot;
  int rooted, double_slash_path;

   
  result = (flags & PATH_NOALLOC) ? path : savestring (path);

   
  if (rooted = ROOTEDPATH(path))
    {
      stub_char = DIRSEP;
#if defined (__CYGWIN__)
      base = (ISALPHA((unsigned char)result[0]) && result[1] == ':') ? result + 3 : result + 1;
#else
      base = result + 1;
#endif
      double_slash_path = DOUBLE_SLASH (path);
      base += double_slash_path;
    }
  else
    {
      stub_char = '.';
#if defined (__CYGWIN__)
      base = (ISALPHA((unsigned char)result[0]) && result[1] == ':') ? result + 2 : result;
#else
      base = result;
#endif
      double_slash_path = 0;
    }

   
  p = q = dotdot = base;

  while (*p)
    {
      if (ISDIRSEP(p[0]))  
	p++;
      else if(p[0] == '.' && PATHSEP(p[1]))	 
	p += 1; 	 
      else if (p[0] == '.' && p[1] == '.' && PATHSEP(p[2]))  
	{
	  p += 2;  
	  if (q > dotdot)	 
	    {
	      if (flags & PATH_CHECKDOTDOT)
		{
		  char c;

		   
		  c = *q;
		  *q = '\0';
		  if (_path_isdir (result) == 0)
		    {
		      if ((flags & PATH_NOALLOC) == 0)
			free (result);
		      return ((char *)NULL);
		    }
		  *q = c;
		}

	      while (--q > dotdot && ISDIRSEP(*q) == 0)
		;
	    }
	  else if (rooted == 0)
	    {
	       
	      if (q != base)
		*q++ = DIRSEP;
	      *q++ = '.';
	      *q++ = '.';
	      dotdot = q;
	    }
	}
      else	 
	{
	   
	  if (q != base)
	    *q++ = DIRSEP;
	  while (*p && (ISDIRSEP(*p) == 0))
	    *q++ = *p++;
	   
	  if (flags & PATH_CHECKEXISTS)
	    {
	      char c;

	       
	      c = *q;
	      *q = '\0';
	      if (_path_isdir (result) == 0)
		{
		  if ((flags & PATH_NOALLOC) == 0)
		    free (result);
		  return ((char *)NULL);
		}
	      *q = c;
	    }
	}
    }

   
  if (q == result)
    *q++ = stub_char;
  *q = '\0';

   
  if (DOUBLE_SLASH(result) && double_slash_path == 0)
    {
      if (result[2] == '\0')	 
	result[1] = '\0';
      else
	memmove (result, result + 1, strlen (result + 1) + 1);
    }

  return (result);
}
