 

 

#include "config.h"

#include "bashtypes.h"
#if !defined (_MINIX) && defined (HAVE_SYS_FILE_H)
#  include <sys/file.h>
#endif
#include "filecntl.h"
#include "posixstat.h"
#include <stdio.h>
#include <errno.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"
#include "bashintl.h"

#include "shell.h"
#include "input.h"
#include "externs.h"
#include "trap.h"

#if !defined (errno)
extern int errno;
#endif  

#if defined (EAGAIN)
#  define X_EAGAIN EAGAIN
#else
#  define X_EAGAIN -99
#endif

#if defined (EWOULDBLOCK)
#  define X_EWOULDBLOCK EWOULDBLOCK
#else
#  define X_EWOULDBLOCK -99
#endif

extern void termsig_handler PARAMS((int));

 

static char localbuf[1024];
static int local_index = 0, local_bufused = 0;

 
int
getc_with_restart (stream)
     FILE *stream;
{
  unsigned char uc;

  CHECK_TERMSIG;

   
  if (local_index == local_bufused || local_bufused == 0)
    {
      while (1)
	{
	  QUIT;
	  run_pending_traps ();

	  local_bufused = read (fileno (stream), localbuf, sizeof(localbuf));
	  if (local_bufused > 0)
	    break;
	  else if (local_bufused == 0)
	    {
	      local_index = 0;
	      return EOF;
	    }
	  else if (errno == X_EAGAIN || errno == X_EWOULDBLOCK)
	    {
	      if (sh_unset_nodelay_mode (fileno (stream)) < 0)
		{
		  sys_error (_("cannot reset nodelay mode for fd %d"), fileno (stream));
		  local_index = local_bufused = 0;
		  return EOF;
		}
	      continue;
	    }
	  else if (errno != EINTR)
	    {
	      local_index = local_bufused = 0;
	      return EOF;
	    }
	  else if (interrupt_state || terminating_signal)	 
	    local_index = local_bufused = 0;
	}
      local_index = 0;
    }
  uc = localbuf[local_index++];
  return uc;
}

int
ungetc_with_restart (c, stream)
     int c;
     FILE *stream;
{
  if (local_index == 0 || c == EOF)
    return EOF;
  localbuf[--local_index] = c;
  return c;
}

#if defined (BUFFERED_INPUT)

 

#if defined (USING_BASH_MALLOC)
#  define MAX_INPUT_BUFFER_SIZE	8172
#else
#  define MAX_INPUT_BUFFER_SIZE	8192
#endif

#if !defined (SEEK_CUR)
#  define SEEK_CUR 1
#endif  

#ifdef max
#  undef max
#endif
#define max(a, b)	(((a) > (b)) ? (a) : (b))
#ifdef min
#  undef min
#endif
#define min(a, b)	((a) > (b) ? (b) : (a))

int bash_input_fd_changed;

 
static BUFFERED_STREAM **buffers = (BUFFERED_STREAM **)NULL;
static int nbuffers;

#define ALLOCATE_BUFFERS(n) \
	do { if ((n) >= nbuffers) allocate_buffers (n); } while (0)

 
static void
allocate_buffers (n)
     int n;
{
  register int i, orig_nbuffers;

  orig_nbuffers = nbuffers;
  nbuffers = n + 20;
  buffers = (BUFFERED_STREAM **)xrealloc
    (buffers, nbuffers * sizeof (BUFFERED_STREAM *));

   
  for (i = orig_nbuffers; i < nbuffers; i++)
    buffers[i] = (BUFFERED_STREAM *)NULL;
}

 
static BUFFERED_STREAM *
make_buffered_stream (fd, buffer, bufsize)
     int fd;
     char *buffer;
     size_t bufsize;
{
  BUFFERED_STREAM *bp;

  bp = (BUFFERED_STREAM *)xmalloc (sizeof (BUFFERED_STREAM));
  ALLOCATE_BUFFERS (fd);
  buffers[fd] = bp;
  bp->b_fd = fd;
  bp->b_buffer = buffer;
  bp->b_size = bufsize;
  bp->b_used = bp->b_inputp = bp->b_flag = 0;
  if (bufsize == 1)
    bp->b_flag |= B_UNBUFF;
  if (O_TEXT && (fcntl (fd, F_GETFL) & O_TEXT) != 0)
    bp->b_flag |= B_TEXT;
  return (bp);
}

 
static BUFFERED_STREAM *
copy_buffered_stream (bp)
     BUFFERED_STREAM *bp;
{
  BUFFERED_STREAM *nbp;

  if (!bp)
    return ((BUFFERED_STREAM *)NULL);

  nbp = (BUFFERED_STREAM *)xmalloc (sizeof (BUFFERED_STREAM));
  xbcopy ((char *)bp, (char *)nbp, sizeof (BUFFERED_STREAM));
  return (nbp);
}

int
set_bash_input_fd (fd)
     int fd;
{
  if (bash_input.type == st_bstream)
    bash_input.location.buffered_fd = fd;
  else if (interactive_shell == 0)
    default_buffered_input = fd;
  return 0;
}

int
fd_is_bash_input (fd)
     int fd;
{
  if (bash_input.type == st_bstream && bash_input.location.buffered_fd == fd)
    return 1;
  else if (interactive_shell == 0 && default_buffered_input == fd)
    return 1;
  return 0;
}

 
int
save_bash_input (fd, new_fd)
     int fd, new_fd;
{
  int nfd;

   
  if (buffers[fd])
    sync_buffered_stream (fd);

   
  nfd = (new_fd == -1) ? fcntl (fd, F_DUPFD, 10) : new_fd;
  if (nfd == -1)
    {
      if (fcntl (fd, F_GETFD, 0) == 0)
	sys_error (_("cannot allocate new file descriptor for bash input from fd %d"), fd);
      return -1;
    }

  if (nfd < nbuffers && buffers[nfd])
    {
       
      internal_error (_("save_bash_input: buffer already exists for new fd %d"), nfd);
      if (buffers[nfd]->b_flag & B_SHAREDBUF)
	buffers[nfd]->b_buffer = (char *)NULL;
      free_buffered_stream (buffers[nfd]);
    }

   
  if (bash_input.type == st_bstream)
    {
      bash_input.location.buffered_fd = nfd;
      fd_to_buffered_stream (nfd);
      close_buffered_fd (fd);	 
    }
  else
     
    bash_input_fd_changed++;

  if (default_buffered_input == fd)
    default_buffered_input = nfd;

  SET_CLOSE_ON_EXEC (nfd);
  return nfd;
}

 
int
check_bash_input (fd)
     int fd;
{
  if (fd_is_bash_input (fd))
    {
      if (fd > 0)
	return ((save_bash_input (fd, -1) == -1) ? -1 : 0);
      else if (fd == 0)
        return ((sync_buffered_stream (fd) == -1) ? -1 : 0);
    }
  return 0;
}
      
 
int
duplicate_buffered_stream (fd1, fd2)
     int fd1, fd2;
{
  int is_bash_input, m;

  if (fd1 == fd2)
    return 0;

  m = max (fd1, fd2);
  ALLOCATE_BUFFERS (m);

   
  is_bash_input = (bash_input.type == st_bstream) &&
		  (bash_input.location.buffered_fd == fd2);

  if (buffers[fd2])
    {
       
      if (buffers[fd1] && buffers[fd1]->b_buffer && buffers[fd1]->b_buffer == buffers[fd2]->b_buffer)
	buffers[fd2] = (BUFFERED_STREAM *)NULL;
       
      else if (buffers[fd2]->b_flag & B_SHAREDBUF)
	{
	  buffers[fd2]->b_buffer = (char *)NULL;
	  free_buffered_stream (buffers[fd2]);
	}
      else
	free_buffered_stream (buffers[fd2]);
    }
  buffers[fd2] = copy_buffered_stream (buffers[fd1]);
  if (buffers[fd2])
    buffers[fd2]->b_fd = fd2;

  if (is_bash_input)
    {
      if (!buffers[fd2])
	fd_to_buffered_stream (fd2);
      buffers[fd2]->b_flag |= B_WASBASHINPUT;
    }

  if (fd_is_bash_input (fd1) || (buffers[fd1] && (buffers[fd1]->b_flag & B_SHAREDBUF)))
    buffers[fd2]->b_flag |= B_SHAREDBUF;

  return (fd2);
}

 
#define fd_is_seekable(fd) (lseek ((fd), 0L, SEEK_CUR) >= 0)

 
BUFFERED_STREAM *
fd_to_buffered_stream (fd)
     int fd;
{
  char *buffer;
  size_t size;
  struct stat sb;

  if (fstat (fd, &sb) < 0)
    {
      close (fd);
      return ((BUFFERED_STREAM *)NULL);
    }

  size = (fd_is_seekable (fd)) ? min (sb.st_size, MAX_INPUT_BUFFER_SIZE) : 1;
  if (size == 0)
    size = 1;
  buffer = (char *)xmalloc (size);

  return (make_buffered_stream (fd, buffer, size));
}

 
BUFFERED_STREAM *
open_buffered_stream (file)
     char *file;
{
  int fd;

  fd = open (file, O_RDONLY);
  return ((fd >= 0) ? fd_to_buffered_stream (fd) : (BUFFERED_STREAM *)NULL);
}

 
void
free_buffered_stream (bp)
     BUFFERED_STREAM *bp;
{
  int n;

  if (!bp)
    return;

  n = bp->b_fd;
  if (bp->b_buffer)
    free (bp->b_buffer);
  free (bp);
  buffers[n] = (BUFFERED_STREAM *)NULL;
}

 
int
close_buffered_stream (bp)
     BUFFERED_STREAM *bp;
{
  int fd;

  if (!bp)
    return (0);
  fd = bp->b_fd;
  if (bp->b_flag & B_SHAREDBUF)
    bp->b_buffer = (char *)NULL;
  free_buffered_stream (bp);
  return (close (fd));
}

 
int
close_buffered_fd (fd)
     int fd;
{
  if (fd < 0)
    {
      errno = EBADF;
      return -1;
    }
  if (fd >= nbuffers || !buffers || !buffers[fd])
    return (close (fd));
  return (close_buffered_stream (buffers[fd]));
}

 
BUFFERED_STREAM *
set_buffered_stream (fd, bp)
     int fd;
     BUFFERED_STREAM *bp;
{
  BUFFERED_STREAM *ret;

  ret = buffers[fd];
  buffers[fd] = bp;
  return ret;
}

 
static int
b_fill_buffer (bp)
     BUFFERED_STREAM *bp;
{
  ssize_t nr;
  off_t o;

  CHECK_TERMSIG;
   
  if ((bp->b_flag & (B_TEXT | B_UNBUFF)) == B_TEXT)
    {
      o = lseek (bp->b_fd, 0, SEEK_CUR);
      nr = zread (bp->b_fd, bp->b_buffer, bp->b_size);
      if (nr > 0 && nr < lseek (bp->b_fd, 0, SEEK_CUR) - o)
	{
	  lseek (bp->b_fd, o, SEEK_SET);
	  bp->b_flag |= B_UNBUFF;
	  bp->b_size = 1;
	  nr = zread (bp->b_fd, bp->b_buffer, bp->b_size);
	}
    }
  else
    nr = zread (bp->b_fd, bp->b_buffer, bp->b_size);
  if (nr <= 0)
    {
      bp->b_used = bp->b_inputp = 0;
      bp->b_buffer[0] = 0;
      if (nr == 0)
	bp->b_flag |= B_EOF;
      else
	bp->b_flag |= B_ERROR;
      return (EOF);
    }

  bp->b_used = nr;
  bp->b_inputp = 0;
  return (bp->b_buffer[bp->b_inputp++] & 0xFF);
}

 
#define bufstream_getc(bp) \
  (bp->b_inputp == bp->b_used || !bp->b_used) \
  		? b_fill_buffer (bp) \
		: bp->b_buffer[bp->b_inputp++] & 0xFF

 
static int
bufstream_ungetc(c, bp)
     int c;
     BUFFERED_STREAM *bp;
{
  if (c == EOF || bp == 0 || bp->b_inputp == 0)
    return (EOF);

  bp->b_buffer[--bp->b_inputp] = c;
  return (c);
}

 
int
sync_buffered_stream (bfd)
     int bfd;
{
  BUFFERED_STREAM *bp;
  off_t chars_left;

  if (buffers == 0 || (bp = buffers[bfd]) == 0)
    return (-1);

  chars_left = bp->b_used - bp->b_inputp;
  if (chars_left)
    lseek (bp->b_fd, -chars_left, SEEK_CUR);
  bp->b_used = bp->b_inputp = 0;
  return (0);
}

int
buffered_getchar ()
{
  CHECK_TERMSIG;

  if (bash_input.location.buffered_fd < 0 || buffers[bash_input.location.buffered_fd] == 0)
    return EOF;

#if !defined (DJGPP)
  return (bufstream_getc (buffers[bash_input.location.buffered_fd]));
#else
   
  int ch;
  while ((ch = bufstream_getc (buffers[bash_input.location.buffered_fd])) == '\r')
    ;
  return ch;
#endif
}

int
buffered_ungetchar (c)
     int c;
{
  return (bufstream_ungetc (c, buffers[bash_input.location.buffered_fd]));
}

 
void
with_input_from_buffered_stream (bfd, name)
     int bfd;
     char *name;
{
  INPUT_STREAM location;
  BUFFERED_STREAM *bp;

  location.buffered_fd = bfd;
   
  bp = fd_to_buffered_stream (bfd);
  init_yy_io (bp == 0 ? return_EOF : buffered_getchar,
	      buffered_ungetchar, st_bstream, name, location);
}

#if defined (TEST)
void *
xmalloc(s)
int s;
{
	return (malloc (s));
}

void *
xrealloc(s, size)
char	*s;
int	size;
{
	if (!s)
		return(malloc (size));
	else
		return(realloc (s, size));
}

void
init_yy_io ()
{
}

process(bp)
BUFFERED_STREAM *bp;
{
	int c;

	while ((c = bufstream_getc(bp)) != EOF)
		putchar(c);
}

BASH_INPUT bash_input;

struct stat dsb;		 

 
main(argc, argv)
int	argc;
char	**argv;
{
	register int i;
	BUFFERED_STREAM *bp;

	if (argc == 1) {
		bp = fd_to_buffered_stream (0);
		process(bp);
		exit(0);
	}
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '\0') {
			bp = fd_to_buffered_stream (0);
			if (!bp)
				continue;
			process(bp);
			free_buffered_stream (bp);
		} else {
			bp = open_buffered_stream (argv[i]);
			if (!bp)
				continue;
			process(bp);
			close_buffered_stream (bp);
		}
	}
	exit(0);
}
#endif  
#endif  
