 

 

#include <config.h>

#if defined (CAN_REDEFINE_GETENV)

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <bashansi.h>
#include <errno.h>
#include <shell.h>

#ifndef errno
extern int errno;
#endif

extern char **environ;

 

 
static char *last_tempenv_value = (char *)NULL;

char *
getenv (name)
     const char *name;
{
  SHELL_VAR *var;

  if (name == 0 || *name == '\0')
    return ((char *)NULL);

  var = find_tempenv_variable ((char *)name);
  if (var)
    {
      FREE (last_tempenv_value);

      last_tempenv_value = value_cell (var) ? savestring (value_cell (var)) : (char *)NULL;
      return (last_tempenv_value);
    }
  else if (shell_variables)
    {
      var = find_variable ((char *)name);
      if (var && exported_p (var))
	return (value_cell (var));
    }
  else if (environ)
    {
      register int i, len;

       

      for (i = 0, len = strlen (name); environ[i]; i++)
	{
	  if ((STREQN (environ[i], name, len)) && (environ[i][len] == '='))
	    return (environ[i] + len + 1);
	}
    }

  return ((char *)NULL);
}

 
char *
_getenv (name)
     const char *name;
{
  return (getenv (name));
}

 
int
putenv (str)
#ifndef HAVE_STD_PUTENV
     const char *str;
#else
     char *str;
#endif
{
  SHELL_VAR *var;
  char *name, *value;
  int offset;

  if (str == 0 || *str == '\0')
    {
      errno = EINVAL;
      return -1;
    }

  offset = assignment (str, 0);
  if (str[offset] != '=')
    {
      errno = EINVAL;
      return -1;
    }
  name = savestring (str);
  name[offset] = 0;

  value = name + offset + 1;

   
  var = bind_variable (name, value, 0);
  if (var == 0)
    {
      errno = EINVAL;
      return -1;
    }

  VUNSETATTR (var, att_invisible);
  VSETATTR (var, att_exported);

  return 0;
}

#if 0
int
_putenv (name)
#ifndef HAVE_STD_PUTENV
     const char *name;
#else
     char *name;
#endif
{
  return putenv (name);
}
#endif

int
setenv (name, value, rewrite)
     const char *name;
     const char *value;
     int rewrite;
{
  SHELL_VAR *var;
  char *v;

  if (name == 0 || *name == '\0' || strchr (name, '=') != 0)
    {
      errno = EINVAL;
      return -1;
    }

  var = 0;
  v = (char *)value;	 
   
  if (rewrite == 0)
    var = find_variable (name);

  if (var == 0)
    var = bind_variable (name, v, 0);

  if (var == 0)
    return -1;

  VUNSETATTR (var, att_invisible);
  VSETATTR (var, att_exported);

  return 0;
}

#if 0
int
_setenv (name, value, rewrite)
     const char *name;
     const char *value;
     int rewrite;
{
  return setenv (name, value, rewrite);
}
#endif

 

#ifdef HAVE_STD_UNSETENV
#define UNSETENV_RETURN(N)	return(N)
#define UNSETENV_RETTYPE	int
#else
#define UNSETENV_RETURN(N)	return
#define UNSETENV_RETTYPE	void
#endif

UNSETENV_RETTYPE
unsetenv (name)
     const char *name;
{
  if (name == 0 || *name == '\0' || strchr (name, '=') != 0)
    {
      errno = EINVAL;
      UNSETENV_RETURN(-1);
    }

   
#if 1
  unbind_variable (name);
#else
  SHELL_VAR *v;

  v = find_variable (name);
  if (v)
    VUNSETATTR (v, att_exported);
#endif

  UNSETENV_RETURN(0);
}
#endif  
