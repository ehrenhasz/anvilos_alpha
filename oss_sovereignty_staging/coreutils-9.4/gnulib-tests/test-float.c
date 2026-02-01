 

#include <config.h>

#include <float.h>

 
int a[] = { FLT_RADIX };

 

 
int fb[] =
  {
    FLT_MANT_DIG, FLT_MIN_EXP, FLT_MAX_EXP,
    FLT_DIG, FLT_MIN_10_EXP, FLT_MAX_10_EXP
  };
float fc[] = { FLT_EPSILON, FLT_MIN, FLT_MAX };

 

 
int db[] =
  {
    DBL_MANT_DIG, DBL_MIN_EXP, DBL_MAX_EXP,
    DBL_DIG, DBL_MIN_10_EXP, DBL_MAX_10_EXP
  };
double dc[] = { DBL_EPSILON, DBL_MIN, DBL_MAX };

 

 
int lb[] =
  {
    LDBL_MANT_DIG, LDBL_MIN_EXP, LDBL_MAX_EXP,
    LDBL_DIG, LDBL_MIN_10_EXP, LDBL_MAX_10_EXP
  };
long double lc1 = LDBL_EPSILON;
long double lc2 = LDBL_MIN;
#if 0  
long double lc3 = LDBL_MAX;
#endif

 

#include "fpucw.h"
#include "macros.h"

#if FLT_RADIX == 2

 
static float
pow2f (int n)
{
  int k = n;
  volatile float x = 1;
  volatile float y = 2;
   
  if (k < 0)
    {
      y = 0.5f;
      k = - k;
    }
  while (k > 0)
    {
      if (k != 2 * (k / 2))
        {
          x = x * y;
          k = k - 1;
        }
      if (k == 0)
        break;
      y = y * y;
      k = k / 2;
    }
   
  return x;
}

 
static double
pow2d (int n)
{
  int k = n;
  volatile double x = 1;
  volatile double y = 2;
   
  if (k < 0)
    {
      y = 0.5;
      k = - k;
    }
  while (k > 0)
    {
      if (k != 2 * (k / 2))
        {
          x = x * y;
          k = k - 1;
        }
      if (k == 0)
        break;
      y = y * y;
      k = k / 2;
    }
   
  return x;
}

 
static long double
pow2l (int n)
{
  int k = n;
  volatile long double x = 1;
  volatile long double y = 2;
   
  if (k < 0)
    {
      y = 0.5L;
      k = - k;
    }
  while (k > 0)
    {
      if (k != 2 * (k / 2))
        {
          x = x * y;
          k = k - 1;
        }
      if (k == 0)
        break;
      y = y * y;
      k = k / 2;
    }
   
  return x;
}

 

static void
test_float (void)
{
   
  ASSERT ((FLT_MIN_EXP % 101111) == (FLT_MIN_EXP) % 101111);

   
  ASSERT ((FLT_MIN_10_EXP % 101111) == (FLT_MIN_10_EXP) % 101111);

   
  ASSERT (FLT_MANT_DIG == 24);
  ASSERT (FLT_MIN_EXP == -125);
  ASSERT (FLT_MAX_EXP == 128);

   
  ASSERT (FLT_MIN_10_EXP == - (int) (- (FLT_MIN_EXP - 1) * 0.30103));

   
  ASSERT (FLT_DIG == (int) ((FLT_MANT_DIG - 1) * 0.30103));

   
  ASSERT (FLT_MIN_10_EXP == - (int) (- (FLT_MIN_EXP - 1) * 0.30103));

   
  ASSERT (FLT_MAX_10_EXP == (int) (FLT_MAX_EXP * 0.30103));

   
  {
    volatile float m = FLT_MAX;
    int n;

    ASSERT (m + m > m);
    for (n = 0; n <= 2 * FLT_MANT_DIG; n++)
      {
        volatile float pow2_n = pow2f (n);  
        volatile float x = m + (m / pow2_n);
        if (x > m)
          ASSERT (x + x == x);
        else
          ASSERT (!(x + x == x));
      }
  }

   
  {
    volatile float m = FLT_MIN;
    volatile float x = pow2f (FLT_MIN_EXP - 1);
    ASSERT (m == x);
  }

   
  {
    volatile float e = FLT_EPSILON;
    volatile float me;
    int n;

    me = 1.0f + e;
    ASSERT (me > 1.0f);
    ASSERT (me - 1.0f == e);
    for (n = 0; n <= 2 * FLT_MANT_DIG; n++)
      {
        volatile float half_n = pow2f (- n);  
        volatile float x = me - half_n;
        if (x < me)
          ASSERT (x <= 1.0f);
      }
  }
}

 

static void
test_double (void)
{
   
  ASSERT ((DBL_MIN_EXP % 101111) == (DBL_MIN_EXP) % 101111);

   
  ASSERT ((DBL_MIN_10_EXP % 101111) == (DBL_MIN_10_EXP) % 101111);

   
  ASSERT (DBL_MANT_DIG == 53);
  ASSERT (DBL_MIN_EXP == -1021);
  ASSERT (DBL_MAX_EXP == 1024);

   
  ASSERT (DBL_MIN_10_EXP == - (int) (- (DBL_MIN_EXP - 1) * 0.30103));

   
  ASSERT (DBL_DIG == (int) ((DBL_MANT_DIG - 1) * 0.30103));

   
  ASSERT (DBL_MIN_10_EXP == - (int) (- (DBL_MIN_EXP - 1) * 0.30103));

   
  ASSERT (DBL_MAX_10_EXP == (int) (DBL_MAX_EXP * 0.30103));

   
  {
    volatile double m = DBL_MAX;
    int n;

    ASSERT (m + m > m);
    for (n = 0; n <= 2 * DBL_MANT_DIG; n++)
      {
        volatile double pow2_n = pow2d (n);  
        volatile double x = m + (m / pow2_n);
        if (x > m)
          ASSERT (x + x == x);
        else
          ASSERT (!(x + x == x));
      }
  }

   
  {
    volatile double m = DBL_MIN;
    volatile double x = pow2d (DBL_MIN_EXP - 1);
    ASSERT (m == x);
  }

   
  {
    volatile double e = DBL_EPSILON;
    volatile double me;
    int n;

    me = 1.0 + e;
    ASSERT (me > 1.0);
    ASSERT (me - 1.0 == e);
    for (n = 0; n <= 2 * DBL_MANT_DIG; n++)
      {
        volatile double half_n = pow2d (- n);  
        volatile double x = me - half_n;
        if (x < me)
          ASSERT (x <= 1.0);
      }
  }
}

 

static void
test_long_double (void)
{
   
  ASSERT ((LDBL_MIN_EXP % 101111) == (LDBL_MIN_EXP) % 101111);

   
  ASSERT ((LDBL_MIN_10_EXP % 101111) == (LDBL_MIN_10_EXP) % 101111);

   
  ASSERT (LDBL_MANT_DIG >= DBL_MANT_DIG);
  ASSERT (LDBL_MIN_EXP - LDBL_MANT_DIG <= DBL_MIN_EXP - DBL_MANT_DIG);
  ASSERT (LDBL_MAX_EXP >= DBL_MAX_EXP);

   
  ASSERT (LDBL_DIG == (int)((LDBL_MANT_DIG - 1) * 0.30103));

   
  ASSERT (LDBL_MIN_10_EXP == - (int) (- (LDBL_MIN_EXP - 1) * 0.30103));

   
  ASSERT (LDBL_MAX_10_EXP == (int) (LDBL_MAX_EXP * 0.30103));

   
  {
    volatile long double m = LDBL_MAX;
    int n;

    ASSERT (m + m > m);
    for (n = 0; n <= 2 * LDBL_MANT_DIG; n++)
      {
        volatile long double pow2_n = pow2l (n);  
        volatile long double x = m + (m / pow2_n);
        if (x > m)
          ASSERT (x + x == x);
        else
          ASSERT (!(x + x == x));
      }
  }

   
  {
    volatile long double m = LDBL_MIN;
    volatile long double x = pow2l (LDBL_MIN_EXP - 1);
    ASSERT (m == x);
  }

   
  {
    volatile long double e = LDBL_EPSILON;
    volatile long double me;
    int n;

    me = 1.0L + e;
    ASSERT (me > 1.0L);
    ASSERT (me - 1.0L == e);
    for (n = 0; n <= 2 * LDBL_MANT_DIG; n++)
      {
        volatile long double half_n = pow2l (- n);  
        volatile long double x = me - half_n;
        if (x < me)
          ASSERT (x <= 1.0L);
      }
  }
}

int
main ()
{
  test_float ();
  test_double ();

  {
    DECL_LONG_DOUBLE_ROUNDING

    BEGIN_LONG_DOUBLE_ROUNDING ();

    test_long_double ();

    END_LONG_DOUBLE_ROUNDING ();
  }

  return 0;
}

#else

int
main ()
{
  fprintf (stderr, "Skipping test: FLT_RADIX is not 2.\n");
  return 77;
}

#endif
