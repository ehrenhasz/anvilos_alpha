 

#include <config.h>

 
#ifdef USE_LONG_DOUBLE
 
extern int rpl_isnanl (long double x) _GL_ATTRIBUTE_CONST;
#elif ! defined USE_FLOAT
 
extern int rpl_isnand (double x);
#else  
 
extern int rpl_isnanf (float x);
#endif

#include <float.h>
#include <string.h>

#include "float+.h"

#ifdef USE_LONG_DOUBLE
# define FUNC rpl_isnanl
# define DOUBLE long double
# define MAX_EXP LDBL_MAX_EXP
# define MIN_EXP LDBL_MIN_EXP
# if defined LDBL_EXPBIT0_WORD && defined LDBL_EXPBIT0_BIT
#  define KNOWN_EXPBIT0_LOCATION
#  define EXPBIT0_WORD LDBL_EXPBIT0_WORD
#  define EXPBIT0_BIT LDBL_EXPBIT0_BIT
# endif
# define SIZE SIZEOF_LDBL
# define L_(literal) literal##L
#elif ! defined USE_FLOAT
# define FUNC rpl_isnand
# define DOUBLE double
# define MAX_EXP DBL_MAX_EXP
# define MIN_EXP DBL_MIN_EXP
# if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
#  define KNOWN_EXPBIT0_LOCATION
#  define EXPBIT0_WORD DBL_EXPBIT0_WORD
#  define EXPBIT0_BIT DBL_EXPBIT0_BIT
# endif
# define SIZE SIZEOF_DBL
# define L_(literal) literal
#else  
# define FUNC rpl_isnanf
# define DOUBLE float
# define MAX_EXP FLT_MAX_EXP
# define MIN_EXP FLT_MIN_EXP
# if defined FLT_EXPBIT0_WORD && defined FLT_EXPBIT0_BIT
#  define KNOWN_EXPBIT0_LOCATION
#  define EXPBIT0_WORD FLT_EXPBIT0_WORD
#  define EXPBIT0_BIT FLT_EXPBIT0_BIT
# endif
# define SIZE SIZEOF_FLT
# define L_(literal) literal##f
#endif

#define EXP_MASK ((MAX_EXP - MIN_EXP) | 7)

#define NWORDS \
  ((sizeof (DOUBLE) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
typedef union { DOUBLE value; unsigned int word[NWORDS]; } memory_double;

 

#define IEEE_FLOATING_POINT (FLT_RADIX == 2 && FLT_MANT_DIG == 24 \
                             && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128)

int
FUNC (DOUBLE x)
{
#if defined KNOWN_EXPBIT0_LOCATION && IEEE_FLOATING_POINT
# if defined USE_LONG_DOUBLE && ((defined __ia64 && LDBL_MANT_DIG == 64) || (defined __x86_64__ || defined __amd64__) || (defined __i386 || defined __i386__ || defined _I386 || defined _M_IX86 || defined _X86_)) && !HAVE_SAME_LONG_DOUBLE_AS_DOUBLE
   
  memory_double m;
  unsigned int exponent;

  m.value = x;
  exponent = (m.word[EXPBIT0_WORD] >> EXPBIT0_BIT) & EXP_MASK;
#  ifdef WORDS_BIGENDIAN
   
  if (exponent == 0)
    return 1 & (m.word[0] >> 15);
  else if (exponent == EXP_MASK)
    return (((m.word[0] ^ 0x8000U) << 16) | m.word[1] | (m.word[2] >> 16)) != 0;
  else
    return 1 & ~(m.word[0] >> 15);
#  else
   
  if (exponent == 0)
    return (m.word[1] >> 31);
  else if (exponent == EXP_MASK)
    return ((m.word[1] ^ 0x80000000U) | m.word[0]) != 0;
  else
    return (m.word[1] >> 31) ^ 1;
#  endif
# else
   
#  if defined __SUNPRO_C || defined __ICC || defined _MSC_VER \
      || defined __DECC || defined __TINYC__ \
      || (defined __sgi && !defined __GNUC__)
   
  static DOUBLE zero = L_(0.0);
  memory_double nan;
  DOUBLE plus_inf = L_(1.0) / zero;
  DOUBLE minus_inf = -L_(1.0) / zero;
  nan.value = zero / zero;
#  else
  static memory_double nan = { L_(0.0) / L_(0.0) };
  static DOUBLE plus_inf = L_(1.0) / L_(0.0);
  static DOUBLE minus_inf = -L_(1.0) / L_(0.0);
#  endif
  {
    memory_double m;

     
    m.value = x;
    if (((m.word[EXPBIT0_WORD] ^ nan.word[EXPBIT0_WORD])
         & (EXP_MASK << EXPBIT0_BIT))
        == 0)
      return (memcmp (&m.value, &plus_inf, SIZE) != 0
              && memcmp (&m.value, &minus_inf, SIZE) != 0);
    else
      return 0;
  }
# endif
#else
   
  if (x == x)
    {
# if defined USE_LONG_DOUBLE && ((defined __ia64 && LDBL_MANT_DIG == 64) || (defined __x86_64__ || defined __amd64__) || (defined __i386 || defined __i386__ || defined _I386 || defined _M_IX86 || defined _X86_)) && !HAVE_SAME_LONG_DOUBLE_AS_DOUBLE
       
      memory_double m1;
      memory_double m2;

      memset (&m1.value, 0, SIZE);
      memset (&m2.value, 0, SIZE);
      m1.value = x;
      m2.value = x + (x ? 0.0L : -0.0L);
      if (memcmp (&m1.value, &m2.value, SIZE) != 0)
        return 1;
# endif
      return 0;
    }
  else
    return 1;
#endif
}
