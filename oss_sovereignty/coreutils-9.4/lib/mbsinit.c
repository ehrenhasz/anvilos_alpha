 
#include <wchar.h>


#if GNULIB_defined_mbstate_t

 

static_assert (sizeof (mbstate_t) >= 4);

int
mbsinit (const mbstate_t *ps)
{
  const char *pstate = (const char *)ps;

  return pstate == NULL || pstate[0] == 0;
}

#else

int
mbsinit (const mbstate_t *ps)
{
# if defined _WIN32 && !defined __CYGWIN__
   
   
  return ps == NULL || *(const unsigned int *)ps == 0;
# else
   
   
  return ps == NULL || *(const char *)ps == 0;
# endif
}

#endif
