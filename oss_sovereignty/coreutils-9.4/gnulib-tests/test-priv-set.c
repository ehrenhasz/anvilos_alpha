 

#include <config.h>

#include "priv-set.h"

#if HAVE_GETPPRIV && HAVE_PRIV_H
# include <priv.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#include "macros.h"

int
main (void)
{
#if HAVE_GETPPRIV && HAVE_PRIV_H
    priv_set_t *set;

    ASSERT (set = priv_allocset ());
    ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
    ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 1);

     
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 1);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 1);
    ASSERT (priv_set_restore (PRIV_PROC_EXEC) == -1);
        ASSERT (errno == EINVAL);
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 1);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 1);
    ASSERT (priv_set_remove (PRIV_PROC_EXEC) == 0);
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 0);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 0);
    ASSERT (priv_set_remove (PRIV_PROC_EXEC) == -1);
        ASSERT (errno == EINVAL);
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 0);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 0);
    ASSERT (priv_set_restore (PRIV_PROC_EXEC) == 0);
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 1);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 1);
    ASSERT (priv_set_restore (PRIV_PROC_EXEC) == -1);
        ASSERT (errno == EINVAL);
    ASSERT (priv_set_ismember (PRIV_PROC_EXEC) == 1);
        ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
        ASSERT (priv_ismember (set, PRIV_PROC_EXEC) == 1);

     
    ASSERT (getppriv (PRIV_EFFECTIVE, set) == 0);
    if (priv_ismember (set, PRIV_SYS_LINKDIR))
      {
        ASSERT (priv_set_restore_linkdir () == -1);
            ASSERT (errno == EINVAL);
        ASSERT (priv_set_remove_linkdir () == 0);
        ASSERT (priv_set_remove_linkdir () == -1);
            ASSERT (errno == EINVAL);
        ASSERT (priv_set_restore_linkdir () == 0);
      }
#else
    ASSERT (priv_set_restore_linkdir () == -1);
    ASSERT (priv_set_remove_linkdir () == -1);
#endif

    return 0;
}
