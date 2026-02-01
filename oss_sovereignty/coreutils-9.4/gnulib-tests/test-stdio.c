 

#include <config.h>

#include <stdio.h>

 
int sk[] = { SEEK_CUR, SEEK_END, SEEK_SET };

 
int pnlm[] = { _PRINTF_NAN_LEN_MAX };

 
static_assert (sizeof NULL == sizeof (void *));

 
fpos_t t1;
off_t t2;
size_t t3;
ssize_t t4;
va_list t5;

#include <string.h>

#include "nan.h"
#include "macros.h"

int
main (void)
{
#if defined DBL_EXPBIT0_WORD && defined DBL_EXPBIT0_BIT
   
  {
    #define NWORDS \
      ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
    typedef union { double value; unsigned int word[NWORDS]; } memory_double;

    double value1;
    memory_double value2;
    char buf[64];

    value1 = NaNd();
    sprintf (buf, "%g", value1);
    ASSERT (strlen (buf) <= _PRINTF_NAN_LEN_MAX);

    value2.value = NaNd ();
    #if DBL_EXPBIT0_BIT == 20
    value2.word[DBL_EXPBIT0_WORD] ^= 0x54321;
    #endif
    sprintf (buf, "%g", value2.value);
    ASSERT (strlen (buf) <= _PRINTF_NAN_LEN_MAX);
  }
#endif

  return 0;
}
