 

#include <config.h>

 
#include "verror.h"

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>

#include "error.h"
#include "xvasprintf.h"

#if ENABLE_NLS
# include "gettext.h"
# define _(msgid) gettext (msgid)
#endif

#ifndef _
# define _(String) String
#endif

 
void
verror (int status, int errnum, const char *format, va_list args)
{
  verror_at_line (status, errnum, NULL, 0, format, args);
}

 
void
verror_at_line (int status, int errnum, const char *file,
                unsigned int line_number, const char *format, va_list args)
{
  char *message = xvasprintf (format, args);
  if (message)
    {
       
      if (file)
        error_at_line (status, errnum, file, line_number, "%s", message);
      else
        error (status, errnum, "%s", message);
    }
  else
    {
       
      error (0, errno, _("unable to display error message"));
      abort ();
    }
  free (message);
}
