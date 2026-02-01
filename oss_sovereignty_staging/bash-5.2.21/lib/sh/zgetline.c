 

 

#include <config.h>

#include <sys/types.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <errno.h>
#include "xmalloc.h"

#if !defined (errno)
extern int errno;
#endif

extern ssize_t zread PARAMS((int, char *, size_t));
extern ssize_t zreadc PARAMS((int, char *));
extern ssize_t zreadintr PARAMS((int, char *, size_t));
extern ssize_t zreadcintr PARAMS((int, char *));

typedef ssize_t breadfunc_t PARAMS((int, char *, size_t));
typedef ssize_t creadfunc_t PARAMS((int, char *));

 
#define GET_LINE_INITIAL_ALLOCATION 16

 

ssize_t
zgetline (fd, lineptr, n, delim, unbuffered_read)
     int fd;
     char **lineptr;
     size_t *n;
     int delim;
     int unbuffered_read;
{
  int retval;
  size_t nr;
  char *line, c;

  if (lineptr == 0 || n == 0 || (*lineptr == 0 && *n != 0))
    return -1;

  nr = 0;
  line = *lineptr;
  
  while (1)
    {
      retval = unbuffered_read ? zread (fd, &c, 1) : zreadc(fd, &c);

      if (retval <= 0)
	{
	  if (line && nr > 0)
	    line[nr] = '\0';
	  break;
	}

      if (nr + 2 >= *n)
	{
	  size_t new_size;

	  new_size = (*n == 0) ? GET_LINE_INITIAL_ALLOCATION : *n * 2;
	  line = (*n >= new_size) ? NULL : xrealloc (*lineptr, new_size);

	  if (line)
	    {
	      *lineptr = line;
	      *n = new_size;
	    }
	  else
	    {
	      if (*n > 0)
		{
		  (*lineptr)[*n - 1] = '\0';
		  nr = *n - 2;
		}
	      break;
	    }
	}

      line[nr] = c;
      nr++;

      if (c == delim)
	{
	  line[nr] = '\0';
	  break;
	}
    }

  return nr - 1;
}
