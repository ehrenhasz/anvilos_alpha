 

#include <config.h>

#include "af_alg.h"

#if USE_LINUX_CRYPTO_API

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <linux/if_alg.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <sys/socket.h>

#include "sys-limits.h"

#define BLOCKSIZE 32768

 
static int
alg_socket (char const *alg)
{
  struct sockaddr_alg salg = {
    .salg_family = AF_ALG,
    .salg_type = "hash",
  };
   
  for (size_t i = 0; (salg.salg_name[i] = alg[i]) != '\0'; i++)
    if (i == sizeof salg.salg_name - 1)
       
      return -EINVAL;

  int cfd = socket (AF_ALG, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
  if (cfd < 0)
    return -EAFNOSUPPORT;
  int ofd = (bind (cfd, (struct sockaddr *) &salg, sizeof salg) == 0
             ? accept4 (cfd, NULL, 0, SOCK_CLOEXEC)
             : -1);
  close (cfd);
  return ofd < 0 ? -EAFNOSUPPORT : ofd;
}

int
afalg_buffer (const char *buffer, size_t len, const char *alg,
              void *resblock, ssize_t hashlen)
{
   
  int fd = fileno (stream);
  int result;
  struct stat st;
  off_t off = ftello (stream);
  if (0 <= off && fstat (fd, &st) == 0
      && (S_ISREG (st.st_mode) || S_TYPEISSHM (&st) || S_TYPEISTMO (&st))
      && off < st.st_size && st.st_size - off < SYS_BUFSIZE_MAX)
    {
       
      if (fflush (stream))
        result = -EIO;
      else
        {
          off_t nbytes = st.st_size - off;
          if (sendfile (ofd, fd, &off, nbytes) == nbytes)
            {
              if (read (ofd, resblock, hashlen) == hashlen)
                {
                   
                  if (lseek (fd, off, SEEK_SET) != (off_t)-1)
                    result = 0;
                  else
                     
                    result = -EAFNOSUPPORT;
                }
              else
                 
                result = -EAFNOSUPPORT;
            }
          else
             
            result = -EAFNOSUPPORT;
       }
    }
  else
    {
       

       
      off_t nseek = 0;

      for (;;)
        {
          char buf[BLOCKSIZE];
           
          size_t blocksize = (nseek == 0 && off < 0 ? 1 : BLOCKSIZE);
          ssize_t size = fread (buf, 1, blocksize, stream);
          if (size == 0)
            {
               
              result = ferror (stream) ? -EIO : nseek == 0 ? -EAFNOSUPPORT : 0;
              break;
            }
          nseek -= size;
          if (send (ofd, buf, size, MSG_MORE) != size)
            {
              if (nseek == -1)
                {
                   
                  ungetc ((unsigned char) buf[0], stream);
                  result = -EAFNOSUPPORT;
                }
              else if (fseeko (stream, nseek, SEEK_CUR) == 0)
                 
                result = -EAFNOSUPPORT;
              else
                result = -EIO;
              break;
            }

           
            result = -EAFNOSUPPORT;
          else
            result = -EIO;
        }
    }
  close (ofd);
  return result;
}

#endif
