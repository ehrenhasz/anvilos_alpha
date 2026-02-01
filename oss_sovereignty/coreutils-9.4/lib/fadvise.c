 
#if (__GNUC__ == 4 && 6 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#endif

#include <config.h>
#include "fadvise.h"

#include <stdio.h>
#include <fcntl.h>
#include "ignore-value.h"

void
fdadvise (int fd, off_t offset, off_t len, fadvice_t advice)
{
#if HAVE_POSIX_FADVISE
  ignore_value (posix_fadvise (fd, offset, len, advice));
#endif
}

void
fadvise (FILE *fp, fadvice_t advice)
{
  if (fp)
    fdadvise (fileno (fp), 0, 0, advice);
}
