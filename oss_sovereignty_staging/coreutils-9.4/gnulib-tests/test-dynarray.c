 

#include <config.h>

#define DYNARRAY_STRUCT int_sequence
#define DYNARRAY_ELEMENT int
#define DYNARRAY_PREFIX intseq_
#include "dynarray.h"

#include "macros.h"

#define N 100000

static int
value_at (long long int i)
{
  return (i % 13) + ((i * i) % 251);
}

int
main ()
{
  struct int_sequence s;
  int i;

  intseq_init (&s);
  for (i = 0; i < N; i++)
    intseq_add (&s, value_at (i));
  for (i = N - 1; i >= N / 2; i--)
    {
      ASSERT (* intseq_at (&s, i) == value_at (i));
      intseq_remove_last (&s);
    }
  intseq_free (&s);

  return 0;
}
