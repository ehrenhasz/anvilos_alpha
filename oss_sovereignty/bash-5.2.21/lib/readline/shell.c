 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_STRING_H)
#  include <string.h>
#else
#  include <strings.h>
#endif  

#if defined (HAVE_LIMITS_H)
#  include <limits.h>
#endif

#if defined (HAVE_FCNTL_H)
#include <fcntl.h>
#endif
#if defined (HAVE_PWD_H)
#include <pwd.h>
#endif

#include <stdio.h>

#include "rlstdc.h"
#include "rlshell.h"
#include "rldefs.h"

#include "xmalloc.h"

#if defined (HAVE_GETPWUID) && !defined (HAVE_GETPW_DECLS)
extern struct passwd *getpwuid (uid_t);
#endif  

#ifndef NULL
#  define NULL 0
#endif

#ifndef CHAR_BIT
#  define CHAR_BIT 8
#endif

 
#define TYPE_SIGNED(t) (! ((t) 0 < (t) -1))

 
#define INT_STRLEN_BOUND(t) \
  ((sizeof (t) * CHAR_BIT - TYPE_SIGNED (t)) * 302 / 1000 \
   + 1 + TYPE_SIGNED (t))

 

 
char *
sh_single_quote (char *string)
{
  register int c;
  char *result, *r, *s;

  result = (char *)xmalloc (3 + (4 * strlen (string)));
  r = result;
  *r++ = '\'';

  for (s = string; s && (c = *s); s++)
    {
      *r++ = c;

      if (c == '\'')
	{
	  *r++ = '\\';	 
	  *r++ = '\'';
	  *r++ = '\'';	 
	}
    }

  *r++ = '\'';
  *r = '\0';

  return (result);
}

 
static char setenv_buf[INT_STRLEN_BOUND (int) + 1];
static char putenv_buf1[INT_STRLEN_BOUND (int) + 6 + 1];	 
static char putenv_buf2[INT_STRLEN_BOUND (int) + 8 + 1];	 

void
sh_set_lines_and_columns (int lines, int cols)
{
#if defined (HAVE_SETENV)
  sprintf (setenv_buf, "%d", lines);
  setenv ("LINES", setenv_buf, 1);

  sprintf (setenv_buf, "%d", cols);
  setenv ("COLUMNS", setenv_buf, 1);
#else  
#  if defined (HAVE_PUTENV)
  sprintf (putenv_buf1, "LINES=%d", lines);
  putenv (putenv_buf1);

  sprintf (putenv_buf2, "COLUMNS=%d", cols);
  putenv (putenv_buf2);
#  endif  
#endif  
}

char *
sh_get_env_value (const char *varname)
{
  return ((char *)getenv (varname));
}

char *
sh_get_home_dir (void)
{
  static char *home_dir = (char *)NULL;
  struct passwd *entry;

  if (home_dir)
    return (home_dir);

  home_dir = (char *)NULL;
#if defined (HAVE_GETPWUID)
#  if defined (__TANDEM)
  entry = getpwnam (getlogin ());
#  else
  entry = getpwuid (getuid ());
#  endif
  if (entry)
    home_dir = savestring (entry->pw_dir);
#endif

#if defined (HAVE_GETPWENT)
  endpwent ();		 
#endif

  return (home_dir);
}

#if !defined (O_NDELAY)
#  if defined (FNDELAY)
#    define O_NDELAY FNDELAY
#  endif
#endif

int
sh_unset_nodelay_mode (int fd)
{
#if defined (HAVE_FCNTL)
  int flags, bflags;

  if ((flags = fcntl (fd, F_GETFL, 0)) < 0)
    return -1;

  bflags = 0;

#ifdef O_NONBLOCK
  bflags |= O_NONBLOCK;
#endif

#ifdef O_NDELAY
  bflags |= O_NDELAY;
#endif

  if (flags & bflags)
    {
      flags &= ~bflags;
      return (fcntl (fd, F_SETFL, flags));
    }
#endif

  return 0;
}
