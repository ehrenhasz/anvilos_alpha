 

 

#include <config.h>

#if defined (USE_VFPRINTF_EMULATION)

#include <stdio.h>

#if !defined (NULL)
#  if defined (__STDC__)
#    define NULL ((void *)0)
#  else
#    define NULL 0x0
#  endif  
#endif  

 
#include <varargs.h>

int
vfprintf (iop, fmt, ap)
     FILE *iop;
     char *fmt;
     va_list ap;
{
  int len;
  char localbuf[BUFSIZ];

  if (iop->_flag & _IONBF)
    {
      iop->_flag &= ~_IONBF;
      iop->_ptr = iop->_base = localbuf;
      len = _doprnt (fmt, ap, iop);
      (void) fflush (iop);
      iop->_flag |= _IONBF;
      iop->_base = NULL;
      iop->_bufsiz = 0;
      iop->_cnt = 0;
    }
  else
    len = _doprnt (fmt, ap, iop);
  return (ferror (iop) ? EOF : len);
}

 
int
vsprintf (str, fmt, ap)
     char *str, *fmt;
     va_list ap;
{
  FILE f;
  int len;

  f._flag = _IOWRT|_IOSTRG;
  f._ptr = str;
  f._cnt = 32767;
  len = _doprnt (fmt, ap, &f);
  *f._ptr = 0;
  return (len);
}
#endif  
