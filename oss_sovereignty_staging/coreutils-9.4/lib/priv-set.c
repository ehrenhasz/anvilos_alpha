 

#include <config.h>

#define PRIV_SET_INLINE _GL_EXTERN_INLINE

#include "priv-set.h"

#if HAVE_GETPPRIV && HAVE_PRIV_H

# include <errno.h>
# include <priv.h>

 
static priv_set_t *eff_set;

 
static priv_set_t *rem_set;

static bool initialized;

static int
priv_set_initialize (void)
{
  if (! initialized)
    {
      eff_set = priv_allocset ();
      if (!eff_set)
        {
          return -1;
        }
      rem_set = priv_allocset ();
      if (!rem_set)
        {
          priv_freeset (eff_set);
          return -1;
        }
      if (getppriv (PRIV_EFFECTIVE, eff_set) != 0)
        {
          priv_freeset (eff_set);
          priv_freeset (rem_set);
          return -1;
        }
      priv_emptyset (rem_set);
      initialized = true;
    }

  return 0;
}


 
int
priv_set_ismember (const char *priv)
{
  if (! initialized && priv_set_initialize () != 0)
    return -1;

  return priv_ismember (eff_set, priv);
}


 
int
priv_set_remove (const char *priv)
{
  if (! initialized && priv_set_initialize () != 0)
    return -1;

  if (priv_ismember (eff_set, priv))
    {
       
      priv_delset (eff_set, priv);
      if (setppriv (PRIV_SET, PRIV_EFFECTIVE, eff_set) != 0)
        {
          priv_addset (eff_set, priv);
          return -1;
        }
      priv_addset (rem_set, priv);
    }
  else
    {
      errno = EINVAL;
      return -1;
    }

  return 0;
}


 
int
priv_set_restore (const char *priv)
{
  if (! initialized && priv_set_initialize () != 0)
    return -1;

  if (priv_ismember (rem_set, priv))
    {
       
      priv_addset (eff_set, priv);
      if (setppriv (PRIV_SET, PRIV_EFFECTIVE, eff_set) != 0)
        {
          priv_delset (eff_set, priv);
          return -1;
        }
      priv_delset (rem_set, priv);
    }
  else
    {
      errno = EINVAL;
      return -1;
    }

  return 0;
}

#endif
