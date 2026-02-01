 

#include <config.h>

#include <math.h>

 
#ifndef signbit
# error missing declaration
#endif

#include <float.h>
#include <limits.h>

#include "minus-zero.h"
#include "infinity.h"
#include "macros.h"

float zerof = 0.0f;
double zerod = 0.0;
long double zerol = 0.0L;

static void
test_signbitf ()
{
   
  ASSERT (!signbit (3.141f));
  ASSERT (!signbit (3.141e30f));
  ASSERT (!signbit (3.141e-30f));
  ASSERT (signbit (-2.718f));
  ASSERT (signbit (-2.718e30f));
  ASSERT (signbit (-2.718e-30f));
   
  ASSERT (!signbit (0.0f));
  if (1.0f / minus_zerof < 0)
    ASSERT (signbit (minus_zerof));
  else
    ASSERT (!signbit (minus_zerof));
   
  ASSERT (!signbit (Infinityf ()));
  ASSERT (signbit (- Infinityf ()));
   
  (void) signbit (zerof / zerof);
#if defined FLT_EXPBIT0_WORD && defined FLT_EXPBIT0_BIT
   
  {
    #define NWORDS \
      ((sizeof (float) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
    typedef union { float value; unsigned int word[NWORDS]; } memory_float;
    memory_float m;
    m.value = zerof / zerof;
# if FLT_EXPBIT0_BIT > 0
    m.word[FLT_EXPBIT0_WORD] ^= (unsigned int) 1 << (FLT_EXPBIT0_BIT - 1);
# else
    m.word[FLT_EXPBIT0_WORD + (FLT_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      ^= (unsigned int) 1 << (sizeof (unsigned int) * CHAR_BIT - 1);
# endif
    if (FLT_EXPBIT0_WORD < NWORDS / 2)
      m.word[FLT_EXPBIT0_WORD + 1] |= (unsigned int) 1 << FLT_EXPBIT0_BIT;
    else
      m.word[0] |= (unsigned int) 1;
    (void) signbit (m.value);
    #undef NWORDS
  }
#endif
}

static void
test_signbitd ()
{
   
  ASSERT (!signbit (3.141));
  ASSERT (!signbit (3.141e30));
  ASSERT (!signbit (3.141e-30));
  ASSERT (signbit (-2.718));
  ASSERT (signbit (-2.718e30));
  ASSERT (signbit (-2.718e-30));
   
  ASSERT (!signbit (0.0));
  if (1.0 / minus_zerod < 0)
    ASSERT (signbit (minus_zerod));
  else
    ASSERT (!signbit (minus_zerod));
   
  ASSERT (!signbit (Infinityd ()));
  ASSERT (signbit (- Infinityd ()));
   
  (void) signbit (zerod / zerod);
#if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
   
  {
    #define NWORDS \
      ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
    typedef union { double value; unsigned int word[NWORDS]; } memory_double;
    memory_double m;
    m.value = zerod / zerod;
# if DBL_EXPBIT0_BIT > 0
    m.word[DBL_EXPBIT0_WORD] ^= (unsigned int) 1 << (DBL_EXPBIT0_BIT - 1);
# else
    m.word[DBL_EXPBIT0_WORD + (DBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      ^= (unsigned int) 1 << (sizeof (unsigned int) * CHAR_BIT - 1);
# endif
    m.word[DBL_EXPBIT0_WORD + (DBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      |= (unsigned int) 1 << DBL_EXPBIT0_BIT;
    (void) signbit (m.value);
    #undef NWORDS
  }
#endif
}

static void
test_signbitl ()
{
   
  ASSERT (!signbit (3.141L));
  ASSERT (!signbit (3.141e30L));
  ASSERT (!signbit (3.141e-30L));
  ASSERT (signbit (-2.718L));
  ASSERT (signbit (-2.718e30L));
  ASSERT (signbit (-2.718e-30L));
   
  ASSERT (!signbit (0.0L));
  if (1.0L / minus_zerol < 0)
    ASSERT (signbit (minus_zerol));
  else
    ASSERT (!signbit (minus_zerol));
   
  ASSERT (!signbit (Infinityl ()));
  ASSERT (signbit (- Infinityl ()));
   
  (void) signbit (zerol / zerol);
#if defined LDBL_EXPBIT0_WORD && defined LDBL_EXPBIT0_BIT
   
  {
    #define NWORDS \
      ((sizeof (long double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
    typedef union { long double value; unsigned int word[NWORDS]; } memory_long_double;

#if defined __powerpc__ && LDBL_MANT_DIG == 106
     
    #undef NWORDS
    #define NWORDS \
      ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
#endif

    memory_long_double m;
    m.value = zerol / zerol;
# if LDBL_EXPBIT0_BIT > 0
    m.word[LDBL_EXPBIT0_WORD] ^= (unsigned int) 1 << (LDBL_EXPBIT0_BIT - 1);
# else
    m.word[LDBL_EXPBIT0_WORD + (LDBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      ^= (unsigned int) 1 << (sizeof (unsigned int) * CHAR_BIT - 1);
# endif
    m.word[LDBL_EXPBIT0_WORD + (LDBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      |= (unsigned int) 1 << LDBL_EXPBIT0_BIT;
    (void) signbit (m.value);
    #undef NWORDS
  }
#endif
}

int
main ()
{
  test_signbitf ();
  test_signbitd ();
  test_signbitl ();
  return 0;
}
