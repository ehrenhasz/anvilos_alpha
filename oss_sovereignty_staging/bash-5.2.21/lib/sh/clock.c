 

 

#include <config.h>

#if defined (HAVE_TIMES)

#include <sys/types.h>
#include <posixtime.h>

#if defined (HAVE_SYS_TIMES_H)
#  include <sys/times.h>
#endif

#include <stdio.h>
#include <stdc.h>

#include <bashintl.h>

#ifndef locale_decpoint
extern int locale_decpoint PARAMS((void));
#endif

extern long get_clk_tck PARAMS((void));

void
clock_t_to_secs (t, sp, sfp)
     clock_t t;
     time_t *sp;
     int *sfp;
{
  static long clk_tck = -1;

  if (clk_tck == -1)
    clk_tck = get_clk_tck ();

  *sfp = t % clk_tck;
  *sfp = (*sfp * 1000) / clk_tck;

  *sp = t / clk_tck;

   
  if (*sfp >= 1000)
    {
      *sp += 1;
      *sfp -= 1000;
    }
}

 
void
print_clock_t (fp, t)
     FILE *fp;
     clock_t t;
{
  time_t timestamp;
  long minutes;
  int seconds, seconds_fraction;

  clock_t_to_secs (t, &timestamp, &seconds_fraction);

  minutes = timestamp / 60;
  seconds = timestamp % 60;

  fprintf (fp, "%ldm%d%c%03ds",  minutes, seconds, locale_decpoint(), seconds_fraction);
}
#endif  
