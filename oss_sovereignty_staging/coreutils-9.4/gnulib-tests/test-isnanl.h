 

#include <float.h>
#include <limits.h>

#include "minus-zero.h"
#include "infinity.h"
#include "nan.h"
#include "macros.h"

int
main ()
{
  #define NWORDS \
    ((sizeof (long double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
  typedef union { unsigned int word[NWORDS]; long double value; }
          memory_long_double;

   
  ASSERT (!isnanl (3.141L));
  ASSERT (!isnanl (3.141e30L));
  ASSERT (!isnanl (3.141e-30L));
  ASSERT (!isnanl (-2.718L));
  ASSERT (!isnanl (-2.718e30L));
  ASSERT (!isnanl (-2.718e-30L));
  ASSERT (!isnanl (0.0L));
  ASSERT (!isnanl (minus_zerol));
   
  ASSERT (!isnanl (Infinityl ()));
  ASSERT (!isnanl (- Infinityl ()));
   
  ASSERT (isnanl (NaNl ()));

#if defined LDBL_EXPBIT0_WORD && defined LDBL_EXPBIT0_BIT
   
  {
#if defined __powerpc__ && LDBL_MANT_DIG == 106
     
    #undef NWORDS
    #define NWORDS \
      ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
#endif

    memory_long_double m;
    m.value = NaNl ();
# if LDBL_EXPBIT0_BIT > 0
    m.word[LDBL_EXPBIT0_WORD] ^= (unsigned int) 1 << (LDBL_EXPBIT0_BIT - 1);
# else
    m.word[LDBL_EXPBIT0_WORD + (LDBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      ^= (unsigned int) 1 << (sizeof (unsigned int) * CHAR_BIT - 1);
# endif
    m.word[LDBL_EXPBIT0_WORD + (LDBL_EXPBIT0_WORD < NWORDS / 2 ? 1 : - 1)]
      |= (unsigned int) 1 << LDBL_EXPBIT0_BIT;
    ASSERT (isnanl (m.value));
  }
#endif

#if ((defined __ia64 && LDBL_MANT_DIG == 64) || (defined __x86_64__ || defined __amd64__) || (defined __i386 || defined __i386__ || defined _I386 || defined _M_IX86 || defined _X86_)) && !HAVE_SAME_LONG_DOUBLE_AS_DOUBLE
 
# ifdef WORDS_BIGENDIAN
#  define LDBL80_WORDS(exponent,manthi,mantlo) \
     { ((unsigned int) (exponent) << 16) | ((unsigned int) (manthi) >> 16), \
       ((unsigned int) (manthi) << 16) | ((unsigned int) (mantlo) >> 16),   \
       (unsigned int) (mantlo) << 16                                        \
     }
# else
#  define LDBL80_WORDS(exponent,manthi,mantlo) \
     { mantlo, manthi, exponent }
# endif
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0xFFFF, 0xC3333333, 0x00000000) };
    ASSERT (isnanl (x.value));
  }
  {
     
    static memory_long_double x =
      { LDBL80_WORDS (0xFFFF, 0x83333333, 0x00000000) };
    ASSERT (isnanl (x.value));
  }
   
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0xFFFF, 0x40000001, 0x00000000) };
    ASSERT (isnanl (x.value) || !isnanl (x.value));
  }
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0xFFFF, 0x00000000, 0x00000000) };
    ASSERT (isnanl (x.value) || !isnanl (x.value));
  }
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0x4004, 0x00000000, 0x00000000) };
    ASSERT (isnanl (x.value) || !isnanl (x.value));
  }
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0x4000, 0x63333333, 0x00000000) };
    ASSERT (isnanl (x.value) || !isnanl (x.value));
  }
  {  
    static memory_long_double x =
      { LDBL80_WORDS (0x0000, 0x83333333, 0x00000000) };
    ASSERT (isnanl (x.value) || !isnanl (x.value));
  }
#endif

  return 0;
}
