 

#include <config.h>

#include "mpsort.h"

#include <string.h>

 

typedef int (*comparison_function) (void const *, void const *);

static void mpsort_with_tmp (void const **restrict, size_t,
                             void const **restrict, comparison_function);

 

static void
mpsort_into_tmp (void const **restrict base, size_t n,
                 void const **restrict tmp,
                 comparison_function cmp)
{
  size_t n1 = n / 2;
  size_t n2 = n - n1;
  size_t a = 0;
  size_t alim = n1;
  size_t b = n1;
  size_t blim = n;
  void const *ba;
  void const *bb;

  mpsort_with_tmp (base + n1, n2, tmp, cmp);
  mpsort_with_tmp (base, n1, tmp, cmp);

  ba = base[a];
  bb = base[b];

  for (;;)
    if (cmp (ba, bb) <= 0)
      {
        *tmp++ = ba;
        a++;
        if (a == alim)
          {
            a = b;
            alim = blim;
            break;
          }
        ba = base[a];
      }
    else
      {
        *tmp++ = bb;
        b++;
        if (b == blim)
          break;
        bb = base[b];
      }

  memcpy (tmp, base + a, (alim - a) * sizeof *base);
}

 

static void
mpsort_with_tmp (void const **restrict base, size_t n,
                 void const **restrict tmp,
                 comparison_function cmp)
{
  if (n <= 2)
    {
      if (n == 2)
        {
          void const *p0 = base[0];
          void const *p1 = base[1];
          if (! (cmp (p0, p1) <= 0))
            {
              base[0] = p1;
              base[1] = p0;
            }
        }
    }
  else
    {
      size_t n1 = n / 2;
      size_t n2 = n - n1;
      size_t i;
      size_t t = 0;
      size_t tlim = n1;
      size_t b = n1;
      size_t blim = n;
      void const *bb;
      void const *tt;

      mpsort_with_tmp (base + n1, n2, tmp, cmp);

      if (n1 < 2)
        tmp[0] = base[0];
      else
        mpsort_into_tmp (base, n1, tmp, cmp);

      tt = tmp[t];
      bb = base[b];

      for (i = 0; ; )
        if (cmp (tt, bb) <= 0)
          {
            base[i++] = tt;
            t++;
            if (t == tlim)
              break;
            tt = tmp[t];
          }
        else
          {
            base[i++] = bb;
            b++;
            if (b == blim)
              {
                memcpy (base + i, tmp + t, (tlim - t) * sizeof *base);
                break;
              }
            bb = base[b];
          }
    }
}

 

void
mpsort (void const **base, size_t n, comparison_function cmp)
{
  mpsort_with_tmp (base, n, base + n, cmp);
}
