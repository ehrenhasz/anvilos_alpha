 

 

#include <config.h>

#if defined (HAVE_TIMEVAL)

#include <sys/types.h>
#include <posixtime.h>

#include <bashintl.h>
#include <stdc.h>

#ifndef locale_decpoint
extern int locale_decpoint PARAMS((void));
#endif

#include <stdio.h>

struct timeval *
difftimeval (d, t1, t2)
     struct timeval *d, *t1, *t2;
{
  d->tv_sec = t2->tv_sec - t1->tv_sec;
  d->tv_usec = t2->tv_usec - t1->tv_usec;
  if (d->tv_usec < 0)
    {
      d->tv_usec += 1000000;
      d->tv_sec -= 1;
      if (d->tv_sec < 0)		 
	{
	  d->tv_sec = 0;
	  d->tv_usec = 0;
	}
    }
  return d;
}

struct timeval *
addtimeval (d, t1, t2)
     struct timeval *d, *t1, *t2;
{
  d->tv_sec = t1->tv_sec + t2->tv_sec;
  d->tv_usec = t1->tv_usec + t2->tv_usec;
  if (d->tv_usec >= 1000000)
    {
      d->tv_usec -= 1000000;
      d->tv_sec += 1;
    }
  return d;
}

struct timeval *
multimeval (d, m)
     struct timeval *d;
     int m;
{
  time_t t;

  t = d->tv_usec * m;
  d->tv_sec = d->tv_sec * m + t / 1000000;
  d->tv_usec = t % 1000000;
  return d;
}

struct timeval *
divtimeval (d, m)
     struct timeval *d;
     int m;
{
  time_t t;

  t = d->tv_sec;
  d->tv_sec = t / m;
  d->tv_usec = (d->tv_usec + 1000000 * (t % m)) / m;
  return d;
}
  
 
int
timeval_to_cpu (rt, ut, st)
     struct timeval *rt, *ut, *st;	 
{
  struct timeval t1, t2;
  register int i;

  addtimeval (&t1, ut, st);
  t2.tv_sec = rt->tv_sec;
  t2.tv_usec = rt->tv_usec;

  for (i = 0; i < 6; i++)
    {
      if ((t1.tv_sec > 99999999) || (t2.tv_sec > 99999999))
	break;
      t1.tv_sec *= 10;
      t1.tv_sec += t1.tv_usec / 100000;
      t1.tv_usec *= 10;
      t1.tv_usec %= 1000000;
      t2.tv_sec *= 10;
      t2.tv_sec += t2.tv_usec / 100000;
      t2.tv_usec *= 10;
      t2.tv_usec %= 1000000;
    }
  for (i = 0; i < 4; i++)
    {
      if (t1.tv_sec < 100000000)
	t1.tv_sec *= 10;
      else
	t2.tv_sec /= 10;
    }

  return ((t2.tv_sec == 0) ? 0 : t1.tv_sec / t2.tv_sec);
}  

 
void
timeval_to_secs (tvp, sp, sfp)
     struct timeval *tvp;
     time_t *sp;
     int *sfp;
{
  int rest;

  *sp = tvp->tv_sec;

  *sfp = tvp->tv_usec % 1000000;	 
  rest = *sfp % 1000;
  *sfp = (*sfp * 1000) / 1000000;
  if (rest >= 500)
    *sfp += 1;

   
  if (*sfp >= 1000)
    {
      *sp += 1;
      *sfp -= 1000;
    }
}
  
 
void
print_timeval (fp, tvp)
     FILE *fp;
     struct timeval *tvp;
{
  time_t timestamp;
  long minutes;
  int seconds, seconds_fraction;

  timeval_to_secs (tvp, &timestamp, &seconds_fraction);

  minutes = timestamp / 60;
  seconds = timestamp % 60;

  fprintf (fp, "%ldm%d%c%03ds",  minutes, seconds, locale_decpoint (), seconds_fraction);
}

#endif  
