 

#include <config.h>

#include "posixtm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "intprops.h"
#include "macros.h"

struct posixtm_test
{
  char const *in;
  unsigned int syntax_bits;
  bool valid;
  int_least64_t t_expected;
};

 
enum { LY = PDS_CENTURY | PDS_SECONDS };

static struct posixtm_test const T[] =
  {
     
    { "12131415.16",     LY, 1,            0},  
    { "12131415",        LY, 1,            0},  

#if !((defined __APPLE__ && defined __MACH__) || defined __sun \
      || (defined _WIN32 && !defined __CYGWIN__))
     
    { "000001010000.00", LY, 1,
                      - INT64_C (62167219200)}, 
    { "000012312359.59", LY, 1,
                      - INT64_C (62135596801)}, 
#endif
#if !(defined _WIN32 && !defined __CYGWIN__)
    { "000101010000.00", LY, 1,
                      - INT64_C (62135596800)}, 
    { "190112132045.51", LY, 1,
                       - INT64_C (2147483649)}, 
    { "190112132045.52", LY, 1,
                       - INT64_C (2147483648)}, 
    { "190112132045.53", LY, 1,  -2147483647},  
    { "190112132046.52", LY, 1,  -2147483588},  
    { "190112132145.52", LY, 1,  -2147480048},  
    { "190112142045.52", LY, 1,  -2147397248},  
    { "190201132045.52", LY, 1,  -2144805248},  
    { "196912312359.59", LY, 1,           -1},  
#endif
    { "197001010000.00", LY, 1,            0},  
    { "197001010000.01", LY, 1,            1},  
    { "197001010001.00", LY, 1,           60},  
    { "197001010000.60", LY, 1,           60},  
    { "197001010100.00", LY, 1,         3600},  
    { "197001020000.00", LY, 1,        86400},  
    { "197002010000.00", LY, 1,      2678400},  
    { "197101010000.00", LY, 1,     31536000},  
    { "197001000000.00", LY, 0,            0},  
    { "197000010000.00", LY, 0,            0},  
    { "197001010060.00", LY, 0,            0},  
    { "197001012400.00", LY, 0,            0},  
    { "197001320000.00", LY, 0,            0},  
    { "197013010000.00", LY, 0,            0},  
    { "203801190314.06", LY, 1,   2147483646},  
    { "203801190314.07", LY, 1,   2147483647},  
    { "203801190314.08", LY, 1,
                       INT64_C (  2147483648)}, 
#if !(defined _WIN32 && !defined __CYGWIN__)
    { "999912312359.59", LY, 1,
                       INT64_C (253402300799)}, 
#endif
    { "1112131415",      LY, 1,   1323785700},  
    { "1112131415.16",   LY, 1,   1323785716},  
    { "201112131415.16", LY, 1,   1323785716},  
#if !(defined _WIN32 && !defined __CYGWIN__)
    { "191112131415.16", LY, 1,  -1831974284},  
#endif
    { "203712131415.16", LY, 1,   2144326516},  
    { "3712131415.16",   LY, 1,   2144326516},  
    { "6812131415.16",   LY, 1,
                       INT64_C (  3122633716)}, 
#if !(defined _WIN32 && !defined __CYGWIN__)
    { "6912131415.16",   LY, 1,     -1590284},  
#endif
    { "7012131415.16",   LY, 1,     29945716},  
    { "1213141599",      PDS_TRAILING_YEAR,
                             1,    945094500},  
    { "1213141500",      PDS_TRAILING_YEAR,
                             1,    976716900},  
    { NULL,               0, 0,            0}
  };

int
main (void)
{
  unsigned int i;
  int fail = 0;
  char curr_year_str[30];
  struct tm *tm;
  time_t t_now;
  int err;
  size_t n_bytes;

   
  err = setenv ("TZ", "UTC0", 1);
  ASSERT (err == 0);

  t_now = time (NULL);
  ASSERT (t_now != (time_t) -1);
  tm = localtime (&t_now);
  ASSERT (tm);
  n_bytes = strftime (curr_year_str, sizeof curr_year_str, "%Y", tm);
  ASSERT (0 < n_bytes);

  for (i = 0; T[i].in; i++)
    {
      time_t t_out;
      time_t t_exp;
      bool ok;

       
      if (T[i].t_expected < 0 && ! TYPE_SIGNED (time_t))
        {
          printf ("skipping %s: result is negative, "
                  "but your time_t is unsigned\n", T[i].in);
          continue;
        }

      if (! (TYPE_MINIMUM (time_t) <= T[i].t_expected
             && T[i].t_expected <= TYPE_MAXIMUM (time_t)))
        {
          printf ("skipping %s: result is out of range of your time_t\n",
                  T[i].in);
          continue;
        }

      t_exp = T[i].t_expected;

       
      if (8 <= strlen (T[i].in)
          && (T[i].in[8] == '.' || T[i].in[8] == '\0'))
        {
          char tmp_buf[20];
          stpcpy (stpcpy (tmp_buf, curr_year_str), T[i].in);
          ASSERT (posixtime (&t_exp, tmp_buf, T[i].syntax_bits));
        }

      ok = posixtime (&t_out, T[i].in, T[i].syntax_bits);
      if (ok != !!T[i].valid)
        {
          printf ("%s return value mismatch: got %d, expected %d\n",
                  T[i].in, !!ok, T[i].valid);
          fail = 1;
          continue;
        }

      if (!ok)
        continue;

      if (t_out != t_exp)
        {
          printf ("%s mismatch (-: actual; +:expected)\n-%12ld\n+%12ld\n",
                  T[i].in, (long) t_out, (long) t_exp);
          fail = 1;
        }
    }

  return fail;
}

 
