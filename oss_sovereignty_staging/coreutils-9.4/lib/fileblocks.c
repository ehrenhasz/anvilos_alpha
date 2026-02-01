 

#include <config.h>

#include <sys/types.h>

#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#if !HAVE_STRUCT_STAT_ST_BLOCKS && !defined _POSIX_SOURCE && defined BSIZE

# include <unistd.h>

# ifndef NINDIR

#  if defined __DJGPP__
typedef long daddr_t;  
#  endif

 
 
#  define NINDIR (BSIZE / sizeof (daddr_t))
# endif  

 
# define NDIR   10

 

off_t
st_blocks (off_t size)
{
  off_t datablks = size / 512 + (size % 512 != 0);
  off_t indrblks = 0;

  if (datablks > NDIR)
    {
      indrblks = (datablks - NDIR - 1) / NINDIR + 1;

      if (datablks > NDIR + NINDIR)
        {
          indrblks += (datablks - NDIR - NINDIR - 1) / (NINDIR * NINDIR) + 1;

          if (datablks > NDIR + NINDIR + NINDIR * NINDIR)
            indrblks++;
        }
    }

  return datablks + indrblks;
}
#else
 
typedef int textutils_fileblocks_unused;
#endif
