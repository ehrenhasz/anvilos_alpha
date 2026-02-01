 

#include <config.h>

#include "randint.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>


#if TEST
# include <inttypes.h>
# include <stdio.h>

int
main (int argc, char **argv)
{
  randint i;
  randint n = strtoumax (argv[1], nullptr, 10);
  randint choices = strtoumax (argv[2], nullptr, 10);
  char const *name = argv[3];
  struct randint_source *ints = randint_all_new (name, SIZE_MAX);

  for (i = 0; i < n; i++)
    printf ("%"PRIuMAX"\n", randint_choose (ints, choices));

  return (randint_all_free (ints) == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}
#endif


#include "xalloc.h"

 
struct randint_source
{
   
  struct randread_source *source;

   
  randint randnum;
  randint randmax;
};

 

struct randint_source *
randint_new (struct randread_source *source)
{
  struct randint_source *s = xmalloc (sizeof *s);
  s->source = source;
  s->randnum = s->randmax = 0;
  return s;
}

 

struct randint_source *
randint_all_new (char const *name, size_t bytes_bound)
{
  struct randread_source *source = randread_new (name, bytes_bound);
  return (source ? randint_new (source) : nullptr);
}

 

struct randread_source *
randint_get_source (struct randint_source const *s)
{
  return s->source;
}

 
enum { HUGE_BYTES = RANDINT_MAX == UCHAR_MAX };

 
static inline randint shift_left (randint x)
{
  return HUGE_BYTES ? 0 : x << CHAR_BIT;
}


 

randint
randint_genmax (struct randint_source *s, randint genmax)
{
  struct randread_source *source = s->source;
  randint randnum = s->randnum;
  randint randmax = s->randmax;
  randint choices = genmax + 1;

  while (1)
    {
      if (randmax < genmax)
        {
           

          size_t i = 0;
          randint rmax = randmax;
          unsigned char buf[sizeof randnum];

          do
            {
              rmax = shift_left (rmax) + UCHAR_MAX;
              i++;
            }
          while (rmax < genmax);

          randread (source, buf, i);

           

          i = 0;

          do
            {
              randnum = shift_left (randnum) + buf[i];
              randmax = shift_left (randmax) + UCHAR_MAX;
              i++;
            }
          while (randmax < genmax);
        }

      if (randmax == genmax)
        {
          s->randnum = s->randmax = 0;
          return randnum;
        }
      else
        {
           

          randint excess_choices = randmax - genmax;
          randint unusable_choices = excess_choices % choices;
          randint last_usable_choice = randmax - unusable_choices;
          randint reduced_randnum = randnum % choices;

          if (randnum <= last_usable_choice)
            {
              s->randnum = randnum / choices;
              s->randmax = excess_choices / choices;
              return reduced_randnum;
            }

           
          randnum = reduced_randnum;
          randmax = unusable_choices - 1;
        }
    }
}

 

void
randint_free (struct randint_source *s)
{
  explicit_bzero (s, sizeof *s);
  free (s);
}

 

int
randint_all_free (struct randint_source *s)
{
  int r = randread_free (s->source);
  int e = errno;
  randint_free (s);
  errno = e;
  return r;
}
