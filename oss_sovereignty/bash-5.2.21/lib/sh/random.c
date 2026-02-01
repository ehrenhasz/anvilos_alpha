 

 

#include "config.h"

#include "bashtypes.h"

#if defined (HAVE_SYS_RANDOM_H)
#  include <sys/random.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif
#include "filecntl.h"

#include <stdio.h>
#include "bashansi.h"

#include "shell.h"

extern time_t shell_start_time;

extern int last_random_value;

static u_bits32_t intrand32 PARAMS((u_bits32_t));
static u_bits32_t genseed PARAMS((void));

static u_bits32_t brand32 PARAMS((void));
static void sbrand32 PARAMS((u_bits32_t));
static void perturb_rand32 PARAMS((void));

 
static u_bits32_t rseed = 1;

 
static u_bits32_t
intrand32 (last)
     u_bits32_t last;
{
   
  ret = (last == 0) ? 123459876 : last;
  h = ret / 127773;
  l = ret - (127773 * h);
  t = 16807 * l - 2836 * h;
  ret = (t < 0) ? t + 0x7fffffff : t;

  return (ret);
}

static u_bits32_t
genseed ()
{
  struct timeval tv;
  u_bits32_t iv;

  gettimeofday (&tv, NULL);
  iv = (u_bits32_t)seedrand;		 
  iv = tv.tv_sec ^ tv.tv_usec ^ getpid () ^ getppid () ^ current_user.uid ^ iv;
  return (iv);
}

#define BASH_RAND_MAX	32767		 

 
int
brand ()
{
  unsigned int ret;

  rseed = intrand32 (rseed);
  if (shell_compatibility_level > 50)
    ret = (rseed >> 16) ^ (rseed & 65535);
  else
    ret = rseed;
  return (ret & BASH_RAND_MAX);
}

 
void
sbrand (seed)
     unsigned long seed;
{
  rseed = seed;
  last_random_value = 0;
}

void
seedrand ()
{
  u_bits32_t iv;

  iv = genseed ();
  sbrand (iv);
}

static u_bits32_t rseed32 = 1073741823;
static int last_rand32;

static int urandfd = -1;

#define BASH_RAND32_MAX	0x7fffffff	 

 
static u_bits32_t
brand32 ()
{
  u_bits32_t ret;

  rseed32 = intrand32 (rseed32);
  return (rseed32 & BASH_RAND32_MAX);
}

static void
sbrand32 (seed)
     u_bits32_t seed;
{
  last_rand32 = rseed32 = seed;
}

void
seedrand32 ()
{
  u_bits32_t iv;

  iv = genseed ();
  sbrand32 (iv);
}

static void
perturb_rand32 ()
{
  rseed32 ^= genseed ();
}

 
void
urandom_close ()
{
  if (urandfd >= 0)
    close (urandfd);
  urandfd = -1;
}

#if !defined (HAVE_GETRANDOM)
 
#ifndef GRND_NONBLOCK
#  define GRND_NONBLOCK 1
#  define GRND_RANDOM 2
#endif

static ssize_t
getrandom (buf, len, flags)
     void *buf;
     size_t len;
     unsigned int flags;
{
  int oflags;
  ssize_t r;
  static int urand_unavail = 0;

#if HAVE_GETENTROPY
  r = getentropy (buf, len);
  return (r == 0) ? len : -1;
#endif

  if (urandfd == -1 && urand_unavail == 0)
    {
      oflags = O_RDONLY;
      if (flags & GRND_NONBLOCK)
	oflags |= O_NONBLOCK;
      urandfd = open ("/dev/urandom", oflags, 0);
      if (urandfd >= 0)
	SET_CLOSE_ON_EXEC (urandfd);
      else
	{
	  urand_unavail = 1;
	  return -1;
	}
    }
  if (urandfd >= 0 && (r = read (urandfd, buf, len)) == len)
    return (r);
  return -1;
}
#endif
      
u_bits32_t
get_urandom32 ()
{
  u_bits32_t ret;

  if (getrandom ((void *)&ret, sizeof (ret), GRND_NONBLOCK) == sizeof (ret))
    return (last_rand32 = ret);

#if defined (HAVE_ARC4RANDOM)
  ret = arc4random ();
#else
  if (subshell_environment)
    perturb_rand32 ();
  do
    ret = brand32 ();
  while (ret == last_rand32);
#endif
  return (last_rand32 = ret);
}
