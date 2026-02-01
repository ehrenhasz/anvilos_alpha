 

#include <config.h>

#include "file-type.h"

#define N_(msgid) (msgid)

char const *
c_file_type (struct stat const *st)
{
   

   

  if (S_ISREG (st->st_mode))
    return st->st_size == 0 ? N_("regular empty file") : N_("regular file");

  if (S_ISDIR (st->st_mode))
    return N_("directory");

  if (S_ISLNK (st->st_mode))
    return N_("symbolic link");

   

  if (S_TYPEISMQ (st))
    return N_("message queue");

  if (S_TYPEISSEM (st))
    return N_("semaphore");

  if (S_TYPEISSHM (st))
    return N_("shared memory object");

  if (S_TYPEISTMO (st))
    return N_("typed memory object");

   

  if (S_ISBLK (st->st_mode))
    return N_("block special file");

  if (S_ISCHR (st->st_mode))
    return N_("character special file");

  if (S_ISCTG (st->st_mode))
    return N_("contiguous data");

  if (S_ISFIFO (st->st_mode))
    return N_("fifo");

  if (S_ISDOOR (st->st_mode))
    return N_("door");

  if (S_ISMPB (st->st_mode))
    return N_("multiplexed block special file");

  if (S_ISMPC (st->st_mode))
    return N_("multiplexed character special file");

  if (S_ISMPX (st->st_mode))
    return N_("multiplexed file");

  if (S_ISNAM (st->st_mode))
    return N_("named file");

  if (S_ISNWK (st->st_mode))
    return N_("network special file");

  if (S_ISOFD (st->st_mode))
    return N_("migrated file with data");

  if (S_ISOFL (st->st_mode))
    return N_("migrated file without data");

  if (S_ISPORT (st->st_mode))
    return N_("port");

  if (S_ISSOCK (st->st_mode))
    return N_("socket");

  if (S_ISWHT (st->st_mode))
    return N_("whiteout");

  return N_("weird file");
}
