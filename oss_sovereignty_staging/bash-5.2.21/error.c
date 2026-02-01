 

 

#include "config.h"

#include "bashtypes.h"
#include <fcntl.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#if defined (PREFER_STDARG)
#  include <stdarg.h>
#else
#  include <varargs.h>
#endif

#include <stdio.h>

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif  

#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "execute_cmd.h"
#include "flags.h"
#include "input.h"

#if defined (HISTORY)
#  include "bashhist.h"
#endif

extern int executing_line_number PARAMS((void));

#if defined (JOB_CONTROL)
extern pid_t shell_pgrp;
extern int give_terminal_to PARAMS((pid_t, int));
#endif  

#if defined (ARRAY_VARS)
extern const char * const bash_badsub_errmsg;
#endif

static void error_prolog PARAMS((int));

 
#if !defined (MAINTAINER)
#define MAINTAINER "bash-maintainers@gnu.org"
#endif

const char * const the_current_maintainer = MAINTAINER;

int gnu_error_format = 0;

static void
error_prolog (print_lineno)
     int print_lineno;
{
  char *ename;
  int line;

  ename = get_name_for_error ();
  line = (print_lineno && interactive_shell == 0) ? executing_line_number () : -1;

  if (line > 0)
    fprintf (stderr, "%s:%s%d: ", ename, gnu_error_format ? "" : _(" line "), line);
  else
    fprintf (stderr, "%s: ", ename);
}

 
char *
get_name_for_error ()
{
  char *name;
#if defined (ARRAY_VARS)
  SHELL_VAR *bash_source_v;
  ARRAY *bash_source_a;
#endif

  name = (char *)NULL;
  if (interactive_shell == 0)
    {
#if defined (ARRAY_VARS)
      bash_source_v = find_variable ("BASH_SOURCE");
      if (bash_source_v && array_p (bash_source_v) &&
	  (bash_source_a = array_cell (bash_source_v)))
	name = array_reference (bash_source_a, 0);
      if (name == 0 || *name == '\0')	 
#endif
	name = dollar_vars[0];
    }
  if (name == 0 && shell_name && *shell_name)
    name = base_pathname (shell_name);
  if (name == 0)
#if defined (PROGRAM)
    name = PROGRAM;
#else
    name = "bash";
#endif

  return (name);
}

 
void
file_error (filename)
     const char *filename;
{
  report_error ("%s: %s", filename, strerror (errno));
}

void
#if defined (PREFER_STDARG)
programming_error (const char *format, ...)
#else
programming_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;
  char *h;

#if defined (JOB_CONTROL)
  give_terminal_to (shell_pgrp, 0);
#endif  

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");
  va_end (args);

#if defined (HISTORY)
  if (remember_on_history)
    {
      h = last_history_line ();
      fprintf (stderr, _("last command: %s\n"), h ? h : "(null)");
    }
#endif

#if 0
  fprintf (stderr, "Report this to %s\n", the_current_maintainer);
#endif

  fprintf (stderr, _("Aborting..."));
  fflush (stderr);

  abort ();
}

 
void
#if defined (PREFER_STDARG)
report_error (const char *format, ...)
#else
report_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  error_prolog (1);

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
  if (exit_immediately_on_error)
    {
      if (last_command_exit_value == 0)
	last_command_exit_value = EXECUTION_FAILURE;
      exit_shell (last_command_exit_value);
    }
}

void
#if defined (PREFER_STDARG)
fatal_error (const char *format, ...)
#else
fatal_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  error_prolog (0);

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
  sh_exit (2);
}

void
#if defined (PREFER_STDARG)
internal_error (const char *format, ...)
#else
internal_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  error_prolog (1);

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
}

void
#if defined (PREFER_STDARG)
internal_warning (const char *format, ...)
#else
internal_warning (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  error_prolog (1);
  fprintf (stderr, _("warning: "));

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
}

void
#if defined (PREFER_STDARG)
internal_inform (const char *format, ...)
#else
internal_inform (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  error_prolog (1);
   
  fprintf (stderr, _("INFORM: "));

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
}

void
#if defined (PREFER_STDARG)
internal_debug (const char *format, ...)
#else
internal_debug (format, va_alist)
     const char *format;
     va_dcl
#endif
{
#ifdef DEBUG
  va_list args;

  error_prolog (1);
  fprintf (stderr, _("DEBUG warning: "));

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);
#else
  return;
#endif
}

void
#if defined (PREFER_STDARG)
sys_error (const char *format, ...)
#else
sys_error (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  int e;
  va_list args;

  e = errno;
  error_prolog (0);

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, ": %s\n", strerror (e));

  va_end (args);
}

 
void
#if defined (PREFER_STDARG)
parser_error (int lineno, const char *format, ...)
#else
parser_error (lineno, format, va_alist)
     int lineno;
     const char *format;
     va_dcl
#endif
{
  va_list args;
  char *ename, *iname;

  ename = get_name_for_error ();
  iname = yy_input_name ();

  if (interactive)
    fprintf (stderr, "%s: ", ename);
  else if (interactive_shell)
    fprintf (stderr, "%s: %s:%s%d: ", ename, iname, gnu_error_format ? "" : _(" line "), lineno);
  else if (STREQ (ename, iname))
    fprintf (stderr, "%s:%s%d: ", ename, gnu_error_format ? "" : _(" line "), lineno);
  else
    fprintf (stderr, "%s: %s:%s%d: ", ename, iname, gnu_error_format ? "" : _(" line "), lineno);

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);

  if (exit_immediately_on_error)
    exit_shell (last_command_exit_value = 2);
}

#ifdef DEBUG
 
char *
strescape (str)
     const char *str;
{
  char *r, *result;
  unsigned char *s;

  r = result = (char *)xmalloc (strlen (str) * 2 + 1);

  for (s = (unsigned char *)str; s && *s; s++)
    {
      if (*s < ' ')
	{
	  *r++ = '^';
	  *r++ = *s+64;
	}
      else if (*s == 127)
	{
	  *r++ = '^';
	  *r++ = '?';
	}
     else
	*r++ = *s;
    }

  *r = '\0';
  return result;
}

void
#if defined (PREFER_STDARG)
itrace (const char *format, ...)
#else
itrace (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;

  fprintf(stderr, "TRACE: pid %ld: ", (long)getpid());

  SH_VA_START (args, format);

  vfprintf (stderr, format, args);
  fprintf (stderr, "\n");

  va_end (args);

  fflush(stderr);
}

 
void
#if defined (PREFER_STDARG)
trace (const char *format, ...)
#else
trace (format, va_alist)
     const char *format;
     va_dcl
#endif
{
  va_list args;
  static FILE *tracefp = (FILE *)NULL;

  if (tracefp == NULL)
    tracefp = fopen("/tmp/bash-trace.log", "a+");

  if (tracefp == NULL)
    tracefp = stderr;
  else
    fcntl (fileno (tracefp), F_SETFD, 1);      

  fprintf(tracefp, "TRACE: pid %ld: ", (long)getpid());

  SH_VA_START (args, format);

  vfprintf (tracefp, format, args);
  fprintf (tracefp, "\n");

  va_end (args);

  fflush(tracefp);
}

#endif  

 
 
 
 
 


static const char * const cmd_error_table[] = {
	N_("unknown command error"),	 
	N_("bad command type"),		 
	N_("bad connector"),		 
	N_("bad jump"),			 
	0
};

void
command_error (func, code, e, flags)
     const char *func;
     int code, e, flags;	 
{
  if (code > CMDERR_LAST)
    code = CMDERR_DEFAULT;

  programming_error ("%s: %s: %d", func, _(cmd_error_table[code]), e);
}

char *
command_errstr (code)
     int code;
{
  if (code > CMDERR_LAST)
    code = CMDERR_DEFAULT;

  return (_(cmd_error_table[code]));
}

#ifdef ARRAY_VARS
void
err_badarraysub (s)
     const char *s;
{
  report_error ("%s: %s", s, _(bash_badsub_errmsg));
}
#endif

void
err_unboundvar (s)
     const char *s;
{
  report_error (_("%s: unbound variable"), s);
}

void
err_readonly (s)
     const char *s;
{
  report_error (_("%s: readonly variable"), s);
}
