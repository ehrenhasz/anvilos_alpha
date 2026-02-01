 

 

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

#if !defined (MAXSYMLINKS)
#  define MAXSYMLINKS 32
#endif

#if !defined (errno)
extern int errno;
#endif  

extern char *get_working_directory PARAMS((char *));

static int
_path_readlink (path, buf, bufsiz)
     char *path;
     char *buf;
     int bufsiz;
{
#ifdef HAVE_READLINK
  return readlink (path, buf, bufsiz);
#else
  errno = EINVAL;
  return -1;
#endif
}

 

#define DOUBLE_SLASH(p)	((p[0] == '/') && (p[1] == '/') && p[2] != '/')

 

char *
sh_physpath (path, flags)
     char *path;
     int flags;
{
  char tbuf[PATH_MAX+1], linkbuf[PATH_MAX+1];
  char *result, *p, *q, *qsave, *qbase, *workpath;
  int double_slash_path, linklen, nlink;

  linklen = strlen (path);

#if 0
   
  if (linklen >= PATH_MAX)
    return (savestring (path));
#endif

  nlink = 0;
  q = result = (char *)xmalloc (PATH_MAX + 1);

   
  if (linklen >= PATH_MAX)
    workpath = savestring (path);
  else
    {
      workpath = (char *)xmalloc (PATH_MAX + 1);
      strcpy (workpath, path);
    }

   

   
#if defined (__CYGWIN__)
  qbase = (ISALPHA((unsigned char)workpath[0]) && workpath[1] == ':') ? workpath + 3 : workpath + 1;
#else
  qbase = workpath + 1;
#endif
  double_slash_path = DOUBLE_SLASH (workpath);
  qbase += double_slash_path;

  for (p = workpath; p < qbase; )
    *q++ = *p++;
  qbase = q;

   

  while (*p)
    {
      if (ISDIRSEP(p[0]))  
	p++;
      else if(p[0] == '.' && PATHSEP(p[1]))	 
	p += 1; 	 
      else if (p[0] == '.' && p[1] == '.' && PATHSEP(p[2]))  
	{
	  p += 2;  
	  if (q > qbase)
	    {
	      while (--q > qbase && ISDIRSEP(*q) == 0)
		;
	    }
	}
      else	 
	{
	   
	  qsave = q;
	  if (q != qbase)
	    *q++ = DIRSEP;
	  while (*p && (ISDIRSEP(*p) == 0))
	    {
	      if (q - result >= PATH_MAX)
		{
#ifdef ENAMETOOLONG
		  errno = ENAMETOOLONG;
#else
		  errno = EINVAL;
#endif
		  goto error;
		}
		
	      *q++ = *p++;
	    }

	  *q = '\0';

	  linklen = _path_readlink (result, linkbuf, PATH_MAX);
	  if (linklen < 0)	 
	    {
	      if (errno != EINVAL)
		goto error;
	      continue;
	    }

	   
	  nlink++;
	  if (nlink > MAXSYMLINKS)
	    {
#ifdef ELOOP
	      errno = ELOOP;
#else
	      errno = EINVAL;
#endif
error:
	      free (result);
	      free (workpath);
	      return ((char *)NULL);
	    }

	  linkbuf[linklen] = '\0';

	   
	  if ((strlen (p) + linklen + 2) >= PATH_MAX)
	    {
#ifdef ENAMETOOLONG
	      errno = ENAMETOOLONG;
#else
	      errno = EINVAL;
#endif
	      goto error;
	    }

	   
	  strcpy (tbuf, linkbuf);
	  tbuf[linklen] = '/';
	  strcpy (tbuf + linklen, p);
	  strcpy (workpath, tbuf);

	  if (ABSPATH(linkbuf))
	    {
	      q = result;
	       
#if defined (__CYGWIN__)
	      qbase = (ISALPHA((unsigned char)workpath[0]) && workpath[1] == ':') ? workpath + 3 : workpath + 1;
#else
	      qbase = workpath + 1;
#endif
	      double_slash_path = DOUBLE_SLASH (workpath);
	      qbase += double_slash_path;
    
	      for (p = workpath; p < qbase; )
		*q++ = *p++;
	      qbase = q;
	    }
	  else
	    {
	      p = workpath;
	      q = qsave;
	    }
	}
    }

  *q = '\0';
  free (workpath);

   
  if (DOUBLE_SLASH(result) && double_slash_path == 0)
    {
      if (result[2] == '\0')	 
	result[1] = '\0';
      else
	memmove (result, result + 1, strlen (result + 1) + 1);
    }

  return (result);
}

char *
sh_realpath (pathname, resolved)
     const char *pathname;
     char *resolved;
{
  char *tdir, *wd;

  if (pathname == 0 || *pathname == '\0')
    {
      errno = (pathname == 0) ? EINVAL : ENOENT;
      return ((char *)NULL);
    }

  if (ABSPATH (pathname) == 0)
    {
      wd = get_working_directory ("sh_realpath");
      if (wd == 0)
	return ((char *)NULL);
      tdir = sh_makepath (wd, (char *)pathname, 0);
      free (wd);
    }
  else
    tdir = savestring (pathname);

  wd = sh_physpath (tdir, 0);
  free (tdir);

  if (resolved == 0)
    return (wd);

  if (wd)
    {
      strncpy (resolved, wd, PATH_MAX - 1);
      resolved[PATH_MAX - 1] = '\0';
      free (wd);
      return resolved;
    }
  else
    {
      resolved[0] = '\0';
      return wd;
    }
}
