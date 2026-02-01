 
 

#include <config.h>

 
#include <string.h>

#include "signature.h"
SIGNATURE_CHECK (memset_explicit, void *, (void *, int, size_t));

#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "vma-iter.h"
#include "macros.h"

#define SECRET "xyzzy1729"
#define SECRET_SIZE 9

static char zero[SECRET_SIZE] = { 0 };

 
#if 0
# define memset_explicit(a, c, n)  memset (a, c, n)
#endif

 

static char stbuf[SECRET_SIZE];

static void
test_static (void)
{
  memcpy (stbuf, SECRET, SECRET_SIZE);
  memset_explicit (stbuf, 0, SECRET_SIZE);
  ASSERT (memcmp (zero, stbuf, SECRET_SIZE) == 0);
  for (int i = 1; i <= UCHAR_MAX; i++)
    {
      char checkbuf[SECRET_SIZE];
      memset (checkbuf, i, SECRET_SIZE);
      memcpy (stbuf, SECRET, SECRET_SIZE);
      memset_explicit (stbuf, i, SECRET_SIZE);
      ASSERT (memcmp (checkbuf, stbuf, SECRET_SIZE) == 0);
    }
}

 

 
#if VMA_ITERATE_SUPPORTED

struct locals
{
  uintptr_t range_start;
  uintptr_t range_end;
};

static int
vma_iterate_callback (void *data, uintptr_t start, uintptr_t end,
                      unsigned int flags)
{
  struct locals *lp = (struct locals *) data;

   
  if (start <= lp->range_start && end > lp->range_start)
    lp->range_start = (end < lp->range_end ? end : lp->range_end);
  if (start < lp->range_end && end >= lp->range_end)
    lp->range_end = (start > lp->range_start ? start : lp->range_start);

  return 0;
}

static bool
is_range_mapped (uintptr_t range_start, uintptr_t range_end)
{
  struct locals l;

  l.range_start = range_start;
  l.range_end = range_end;
  vma_iterate (vma_iterate_callback, &l);
  return l.range_start == l.range_end;
}

#else

static bool
is_range_mapped (uintptr_t range_start, uintptr_t range_end)
{
  return true;
}

#endif

static void
test_heap (void)
{
  char *heapbuf = (char *) malloc (SECRET_SIZE);
  ASSERT (heapbuf);
  uintptr_t volatile addr = (uintptr_t) heapbuf;
  memcpy (heapbuf, SECRET, SECRET_SIZE);
  memset_explicit (heapbuf, 0, SECRET_SIZE);
  free (heapbuf);
  heapbuf = (char *) addr;
  if (is_range_mapped (addr, addr + SECRET_SIZE))
    {
       
      ASSERT (memcmp (heapbuf, SECRET, SECRET_SIZE) != 0);
      printf ("test_heap: address range is still mapped after free().\n");
    }
  else
    printf ("test_heap: address range is unmapped after free().\n");
}

 

 
static bool _GL_ATTRIBUTE_NOINLINE
do_secret_stuff (int volatile pass, char *volatile *volatile last_stackbuf)
{
  char stackbuf[SECRET_SIZE];
  if (pass == 1)
    {
      memcpy (stackbuf, SECRET, SECRET_SIZE);
      memset_explicit (stackbuf, 0, SECRET_SIZE);
      *last_stackbuf = stackbuf;
      return false;
    }
  else  
    {
       
      return memcmp (zero, *last_stackbuf, SECRET_SIZE) != 0;
    }
}

static void
test_stack (void)
{
  int count = 0;
  int repeat;
  char *volatile last_stackbuf;

  for (repeat = 2 * 1000; repeat > 0; repeat--)
    {
       
      if ((repeat % 2) == 0)
        do_secret_stuff (1, &last_stackbuf);
      else
        count += do_secret_stuff (2, &last_stackbuf);
    }
   
  printf ("test_stack: count = %d\n", count);
  ASSERT (count < 50);
}

 

int
main ()
{
  test_static ();
  test_heap ();
  test_stack ();

  return 0;
}
