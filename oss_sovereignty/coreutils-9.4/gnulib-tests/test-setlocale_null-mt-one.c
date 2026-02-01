 

#include <config.h>

 
#if 4 < __GNUC__ + (3 <= __GNUC_MINOR__)
# pragma GCC diagnostic ignored "-Wreturn-type"
#endif

#if USE_ISOC_THREADS || USE_POSIX_THREADS || USE_ISOC_AND_POSIX_THREADS || USE_WINDOWS_THREADS

 
#include <locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "glthread/thread.h"

 
#undef setlocale


 

#if defined _WIN32 && !defined __CYGWIN__
# define ENGLISH "English_United States"
# define GERMAN  "German_Germany"
# define FRENCH  "French_France"
# define ENCODING ".1252"
#else
# define ENGLISH "en_US"
# define GERMAN  "de_DE"
# define FRENCH  "fr_FR"
# if defined __sgi
#  define ENCODING ".ISO8859-15"
# elif defined __hpux
#  define ENCODING ".utf8"
# else
#  define ENCODING ".UTF-8"
# endif
#endif

static const char LOCALE1[] = ENGLISH ENCODING;
static const char LOCALE2[] = GERMAN ENCODING;
static const char LOCALE3[] = FRENCH ENCODING;

static char *expected;

static void *
thread1_func (void *arg)
{
  for (;;)
    {
      char buf[SETLOCALE_NULL_MAX];

      if (setlocale_null_r (LC_NUMERIC, buf, sizeof (buf)))
        abort ();
      if (strcmp (expected, buf) != 0)
        {
          fprintf (stderr, "thread1 disturbed by thread2!\n"); fflush (stderr);
          abort ();
        }
    }

   
}

static void *
thread2_func (void *arg)
{
  for (;;)
    {
      char buf[SETLOCALE_NULL_MAX];

      setlocale_null_r (LC_NUMERIC, buf, sizeof (buf));
      setlocale_null_r (LC_TIME, buf, sizeof (buf));
    }

   
}

int
main (int argc, char *argv[])
{
  if (setlocale (LC_ALL, LOCALE1) == NULL)
    {
      fprintf (stderr, "Skipping test: LOCALE1 not recognized\n");
      return 77;
    }
  if (setlocale (LC_NUMERIC, LOCALE2) == NULL)
    {
      fprintf (stderr, "Skipping test: LOCALE2 not recognized\n");
      return 77;
    }
  if (setlocale (LC_TIME, LOCALE3) == NULL)
    {
      fprintf (stderr, "Skipping test: LOCALE3 not recognized\n");
      return 77;
    }

  expected = strdup (setlocale (LC_NUMERIC, NULL));

   
  gl_thread_create (thread1_func, NULL);
  gl_thread_create (thread2_func, NULL);

   
  {
    struct timespec duration;
    duration.tv_sec = 2;
    duration.tv_nsec = 0;

    nanosleep (&duration, NULL);
  }

  return 0;
}

#else

 

#include <stdio.h>

int
main ()
{
  fputs ("Skipping test: multithreading not enabled\n", stderr);
  return 77;
}

#endif

 
