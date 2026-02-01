 

#include <config.h>

#include <stdlib.h>
#include <errno.h>

#include "assure.h"
#include "xalloc.h"
#include "xfts.h"

 

FTS *
xfts_open (char * const *argv, int options,
           int (*compar) (const FTSENT **, const FTSENT **))
{
  FTS *fts = fts_open (argv, options | FTS_CWDFD, compar);
  if (fts == nullptr)
    {
       
      affirm (errno != EINVAL);
      xalloc_die ();
    }

  return fts;
}

 
bool
cycle_warning_required (FTS const *fts, FTSENT const *ent)
{
#define ISSET(Fts,Opt) ((Fts)->fts_options & (Opt))
   
  return ((ISSET (fts, FTS_PHYSICAL) && !ISSET (fts, FTS_COMFOLLOW))
          || (ISSET (fts, FTS_PHYSICAL) && ISSET (fts, FTS_COMFOLLOW)
              && ent->fts_level != FTS_ROOTLEVEL));
}
