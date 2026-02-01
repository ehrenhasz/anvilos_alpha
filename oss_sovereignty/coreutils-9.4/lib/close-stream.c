 

#include <config.h>

#include "close-stream.h"

#include <errno.h>

#include "fpending.h"

#if USE_UNLOCKED_IO
# include "unlocked-io.h"
#endif

 

int
close_stream (FILE *stream)
{
  const bool some_pending = (__fpending (stream) != 0);
  const bool prev_fail = (ferror (stream) != 0);
  const bool fclose_fail = (fclose (stream) != 0);

   

  if (prev_fail || (fclose_fail && (some_pending || errno != EBADF)))
    {
      if (! fclose_fail)
        errno = 0;
      return EOF;
    }

  return 0;
}
