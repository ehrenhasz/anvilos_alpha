 

 
#include "count-leading-zeros.h"
#include "stat-size.h"


 
enum { IO_BUFSIZE = 128 * 1024 };
static inline idx_t
io_blksize (struct stat sb)
{
   
  idx_t blocksize = ST_BLKSIZE (sb) <= 0 ? IO_BUFSIZE : ST_BLKSIZE (sb);

   
  blocksize += (IO_BUFSIZE - 1) - (IO_BUFSIZE - 1) % blocksize;

   
  if (S_ISREG (sb.st_mode)
      && blocksize & (blocksize - 1))
    {
      int leading_zeros = count_leading_zeros_ll (blocksize);
      if (IDX_MAX < ULLONG_MAX || leading_zeros)
        {
          unsigned long long power = 1ull << (ULLONG_WIDTH - leading_zeros);
          if (power <= IDX_MAX)
            blocksize = power;
        }
    }

   
  return MIN (MIN (IDX_MAX, SIZE_MAX) / 2 + 1,
              blocksize);
}
