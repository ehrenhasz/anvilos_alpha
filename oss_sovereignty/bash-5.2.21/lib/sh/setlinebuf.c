 

 

#include <config.h>

#include <stdio.h>

#include <xmalloc.h>

#if defined (USING_BASH_MALLOC)
#  define LBUF_BUFSIZE	2016
#else
#  define LBUF_BUFSIZE	BUFSIZ
#endif

static char *stdoutbuf = 0;
static char *stderrbuf = 0;

 
int
sh_setlinebuf (stream)
     FILE *stream;
{
#if !defined (HAVE_SETLINEBUF) && !defined (HAVE_SETVBUF)
  return (0);
#endif

#if defined (HAVE_SETVBUF)
  char *local_linebuf;

#if defined (USING_BASH_MALLOC)
  if (stream == stdout && stdoutbuf == 0)
    local_linebuf = stdoutbuf = (char *)xmalloc (LBUF_BUFSIZE);
  else if (stream == stderr && stderrbuf == 0)
    local_linebuf = stderrbuf = (char *)xmalloc (LBUF_BUFSIZE);
  else
    local_linebuf = (char *)NULL;	 
#else
  local_linebuf = (char *)NULL;
#endif

  return (setvbuf (stream, local_linebuf, _IOLBF, LBUF_BUFSIZE));
#else  

  setlinebuf (stream);
  return (0);

#endif  
}
