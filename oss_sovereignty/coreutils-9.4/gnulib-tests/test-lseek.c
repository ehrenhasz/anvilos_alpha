 

#include <config.h>

#include <unistd.h>

#include "signature.h"
SIGNATURE_CHECK (lseek, off_t, (int, off_t, int));

#include <errno.h>

#include "macros.h"

 
int
main (int argc, char **argv)
{
  if (argc != 2)
    return 2;
  switch (*argv[1])
    {
    case '0':  
      ASSERT (lseek (0, (off_t)2, SEEK_SET) == 2);
      ASSERT (lseek (0, (off_t)-4, SEEK_CUR) == -1);
      ASSERT (errno == EINVAL);
      errno = 0;
#if ! defined __BEOS__
       
      ASSERT (lseek (0, (off_t)0, SEEK_CUR) == 2);
#endif
#if 0  
      ASSERT (lseek (0, (off_t)0, (SEEK_SET | SEEK_CUR | SEEK_END) + 1) == -1);
      ASSERT (errno == EINVAL);
#endif
      ASSERT (lseek (1, (off_t)2, SEEK_SET) == 2);
      errno = 0;
      ASSERT (lseek (1, (off_t)-4, SEEK_CUR) == -1);
      ASSERT (errno == EINVAL);
      errno = 0;
#if ! defined __BEOS__
       
      ASSERT (lseek (1, (off_t)0, SEEK_CUR) == 2);
#endif
#if 0  
      ASSERT (lseek (1, (off_t)0, (SEEK_SET | SEEK_CUR | SEEK_END) + 1) == -1);
      ASSERT (errno == EINVAL);
#endif
      break;

    case '1':  
      errno = 0;
      ASSERT (lseek (0, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == ESPIPE);
      errno = 0;
      ASSERT (lseek (1, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == ESPIPE);
      break;

    case '2':  
       
      close (0);
      close (1);

      errno = 0;
      ASSERT (lseek (0, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == EBADF);

      errno = 0;
      ASSERT (lseek (1, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == EBADF);

       
      errno = 0;
      ASSERT (lseek (-1, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == EBADF);

      close (99);
      errno = 0;
      ASSERT (lseek (99, (off_t)0, SEEK_CUR) == -1);
      ASSERT (errno == EBADF);

      break;

    default:
      return 1;
    }
  return 0;
}
