 

#include <limits.h>

#include "minus-zero.h"
#include "infinity.h"
#include "nan.h"
#include "macros.h"

int
main ()
{
   
  ASSERT (!isnand (3.141));
  ASSERT (!isnand (3.141e30));
  ASSERT (!isnand (3.141e-30));
  ASSERT (!isnand (-2.718));
  ASSERT (!isnand (-2.718e30));
  ASSERT (!isnand (-2.718e-30));
  ASSERT (!isnand (0.0));
  ASSERT (!isnand (minus_zerod));
   
  ASSERT (!isnand (Infinityd ()));
  ASSERT (!isnand (- Infinityd ()));
   
  ASSERT (isnand (NaNd ()));
#if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
   
  {
    #define NWORDS \
      ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
    typedef union { double value; unsigned int word[NWORDS]; } memory_double;
    memory_double m;
    m.value = NaNd ();
# if DBL_EXPBIT0_BIT > 0
    m.word[DBL_EXPBIT0_WORD] ^= (unsigned int) 1 << (DBL_EXPBIT0_BIT - 1);
# else
    m.word[DBL_EXPBIT0_WORD + (DBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      ^= (unsigned int) 1 << (sizeof (unsigned int) * CHAR_BIT - 1);
# endif
    m.word[DBL_EXPBIT0_WORD + (DBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      |= (unsigned int) 1 << DBL_EXPBIT0_BIT;
    ASSERT (isnand (m.value));
  }
#endif
  return 0;
}
