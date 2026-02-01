 

#include <config.h>

#include <sys/random.h>

#include "signature.h"
SIGNATURE_CHECK (getrandom, ssize_t, (void *, size_t, unsigned int));

#include <errno.h>
#include <string.h>

#include "macros.h"

int
main (void)
{
  char buf1[8];
  char buf2[8];
  char large_buf[100000];
  ssize_t ret;

   
  ret = getrandom (buf1, sizeof (buf1), 0);
  if (ret < 0)
    ASSERT (errno == ENOSYS);
  else
    {
      ret = getrandom (buf2, sizeof (buf2), 0);
      if (ret < 0)
        ASSERT (errno == ENOSYS);
      else
        {
           
          ASSERT (memcmp (buf1, buf2, sizeof (buf1)) != 0);
        }
    }

   
  ret = getrandom (buf1, sizeof (buf1), GRND_RANDOM);
  if (ret < 0)
    ASSERT (errno == ENOSYS);
  else
    {
      ret = getrandom (buf2, sizeof (buf2), GRND_RANDOM);
      if (ret < 0)
        ASSERT (errno == ENOSYS);
      else
        {
           
          ASSERT (memcmp (buf1, buf2, sizeof (buf1)) != 0);
        }
    }

   
  ret = getrandom (large_buf, sizeof (large_buf), GRND_RANDOM | GRND_NONBLOCK);
  ASSERT (ret <= (ssize_t) sizeof (large_buf));
   
  if (ret < 0)
    ASSERT (errno == ENOSYS || errno == EAGAIN
            || errno == EINVAL  );
  else
    ASSERT (ret > 0);

  if (getrandom (buf1, 1, 0) < 1)
    if (getrandom (buf1, 1, GRND_RANDOM) < 1)
      {
        fputs ("Skipping test: getrandom is ineffective\n", stderr);
        return 77;
      }

  return 0;
}
