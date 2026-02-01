 

#include <stdio.h>
#include <stdlib.h>

#ifndef FALLTHROUGH
# if (__GNUC__ >= 7) || (__clang_major__ >= 10)
#  define FALLTHROUGH __attribute__ ((__fallthrough__))
# else
#  define FALLTHROUGH ((void) 0)
# endif
#endif

 
#ifndef ASSERT_STREAM
# define ASSERT_STREAM stderr
#endif

 
#define ASSERT(expr) \
  do                                                                         \
    {                                                                        \
      if (!(expr))                                                           \
        {                                                                    \
          fprintf (ASSERT_STREAM, "%s:%d: assertion '%s' failed\n",          \
                   __FILE__, __LINE__, #expr);                               \
          fflush (ASSERT_STREAM);                                            \
          abort ();                                                          \
        }                                                                    \
    }                                                                        \
  while (0)

 
#define ASSERT_NO_STDIO(expr) \
  do                                                        \
    {                                                       \
      if (!(expr))                                          \
        {                                                   \
          WRITE_TO_STDERR (__FILE__);                       \
          WRITE_TO_STDERR (":");                            \
          WRITE_MACROEXPANDED_INTEGER_TO_STDERR (__LINE__); \
          WRITE_TO_STDERR (": assertion '");                \
          WRITE_TO_STDERR (#expr);                          \
          WRITE_TO_STDERR ("' failed\n");                   \
          abort ();                                         \
        }                                                   \
    }                                                       \
  while (0)
#define WRITE_MACROEXPANDED_INTEGER_TO_STDERR(integer) \
  WRITE_INTEGER_TO_STDERR(integer)
#define WRITE_INTEGER_TO_STDERR(integer) \
  WRITE_TO_STDERR (#integer)
#define WRITE_TO_STDERR(string_literal) \
  {                                     \
    const char *s = string_literal;     \
    int ret = write (2, s, strlen (s)); \
    (void) ret;                         \
  }

 
#define SIZEOF(array) (sizeof (array) / sizeof (array[0]))

 
#define STREQ(a, b) (strcmp (a, b) == 0)

 
extern const float randomf[1000];
extern const double randomd[1000];
extern const long double randoml[1000];
