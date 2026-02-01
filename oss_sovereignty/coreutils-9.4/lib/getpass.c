 
# define _GL_ARG_NONNULL(params)
# include <config.h>
#endif

#include "getpass.h"

#include <stdio.h>

#if !(defined _WIN32 && !defined __CYGWIN__)

# if HAVE_DECL___FSETLOCKING && HAVE___FSETLOCKING
#  if HAVE_STDIO_EXT_H
#   include <stdio_ext.h>
#  endif
# else
#  define __fsetlocking(stream, type)     
# endif

# if HAVE_TERMIOS_H
#  include <termios.h>
# endif

# if USE_UNLOCKED_IO
#  include "unlocked-io.h"
# else
#  if !HAVE_DECL_FFLUSH_UNLOCKED
#   undef fflush_unlocked
#   define fflush_unlocked(x) fflush (x)
#  endif
#  if !HAVE_DECL_FLOCKFILE
#   undef flockfile
#   define flockfile(x) ((void) 0)
#  endif
#  if !HAVE_DECL_FUNLOCKFILE
#   undef funlockfile
#   define funlockfile(x) ((void) 0)
#  endif
#  if !HAVE_DECL_FPUTS_UNLOCKED
#   undef fputs_unlocked
#   define fputs_unlocked(str,stream) fputs (str, stream)
#  endif
#  if !HAVE_DECL_PUTC_UNLOCKED
#   undef putc_unlocked
#   define putc_unlocked(c,stream) putc (c, stream)
#  endif
# endif

 

# ifndef TCSASOFT
#  define TCSASOFT 0
# endif

static void
call_fclose (void *arg)
{
  if (arg != NULL)
    fclose (arg);
}

char *
getpass (const char *prompt)
{
  FILE *tty;
  FILE *in, *out;
# if HAVE_TCGETATTR
  struct termios s, t;
# endif
  bool tty_changed = false;
  static char *buf;
  static size_t bufsize;
  ssize_t nread;

   

  tty = fopen ("/dev/tty", "w+e");
  if (tty == NULL)
    {
      in = stdin;
      out = stderr;
    }
  else
    {
       
      __fsetlocking (tty, FSETLOCKING_BYCALLER);

      out = in = tty;
    }

  flockfile (out);

   
# if HAVE_TCGETATTR
  if (tcgetattr (fileno (in), &t) == 0)
    {
       
      s = t;
       
      t.c_lflag &= ~(ECHO | ISIG);
      tty_changed = (tcsetattr (fileno (in), TCSAFLUSH | TCSASOFT, &t) == 0);
    }
# endif

  if (prompt)
    {
       
      fputs_unlocked (prompt, out);
      fflush_unlocked (out);
    }

   
  nread = getline (&buf, &bufsize, in);

   
  fseeko (out, 0, SEEK_CUR);

  if (buf != NULL)
    {
      if (nread < 0)
        buf[0] = '\0';
      else if (buf[nread - 1] == '\n')
        {
           
          buf[nread - 1] = '\0';
          if (tty_changed)
            {
               
              putc_unlocked ('\n', out);
            }
        }
    }

   
# if HAVE_TCSETATTR
  if (tty_changed)
    tcsetattr (fileno (in), TCSAFLUSH | TCSASOFT, &s);
# endif

  funlockfile (out);

  call_fclose (tty);

  return buf;
}

#else  

 

 
# include <limits.h>
 
# include <conio.h>
 
# include <string.h>

# ifndef PASS_MAX
#  define PASS_MAX 512
# endif

char *
getpass (const char *prompt)
{
  char getpassbuf[PASS_MAX + 1];
  size_t i = 0;
  int c;

  if (prompt)
    {
      fputs (prompt, stderr);
      fflush (stderr);
    }

  for (;;)
    {
      c = _getch ();
      if (c == '\r')
        {
          getpassbuf[i] = '\0';
          break;
        }
      else if (i < PASS_MAX)
        {
          getpassbuf[i++] = c;
        }

      if (i >= PASS_MAX)
        {
          getpassbuf[i] = '\0';
          break;
        }
    }

  if (prompt)
    {
      fputs ("\r\n", stderr);
      fflush (stderr);
    }

  return strdup (getpassbuf);
}
#endif
