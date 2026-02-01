 

#include <config.h>

 
#define _NETBSD_SOURCE 1

 
#include <string.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#if !HAVE_SNPRINTF
# include <stdarg.h>
#endif

#include "strerror-override.h"

#if STRERROR_R_CHAR_P

# if HAVE___XPG_STRERROR_R
_GL_EXTERN_C int __xpg_strerror_r (int errnum, char *buf, size_t buflen);
# endif

#elif HAVE_DECL_STRERROR_R

 

# include <limits.h>

#else

 
# undef strerror

# if defined __NetBSD__ || defined __hpux || (defined _WIN32 && !defined __CYGWIN__) || defined __sgi || (defined __sun && !defined _LP64) || defined __CYGWIN__

 

 
#  if HAVE_CATGETS
#   include <nl_types.h>
#  endif

#ifdef __cplusplus
extern "C" {
#endif

 
#  if defined __hpux || defined __sgi
extern int sys_nerr;
extern char *sys_errlist[];
#  endif

 
#  if defined __sun && !defined _LP64
extern int sys_nerr;
#  endif

#ifdef __cplusplus
}
#endif

# else

#  include "glthread/lock.h"

 
gl_lock_define_initialized(static, strerror_lock)

# endif

#endif

 
#if !HAVE_SNPRINTF
static int
local_snprintf (char *buf, size_t buflen, const char *format, ...)
{
  va_list args;
  int result;

  va_start (args, format);
  result = _vsnprintf (buf, buflen, format, args);
  va_end (args);
  if (buflen > 0 && (result < 0 || result >= buflen))
    buf[buflen - 1] = '\0';
  return result;
}
# undef snprintf
# define snprintf local_snprintf
#endif

 
static int
safe_copy (char *buf, size_t buflen, const char *msg)
{
  size_t len = strlen (msg);
  size_t moved = len < buflen ? len : buflen - 1;

   
  memmove (buf, msg, moved);
  buf[moved] = '\0';
  return len < buflen ? 0 : ERANGE;
}


int
strerror_r (int errnum, char *buf, size_t buflen)
#undef strerror_r
{
   
  if (buflen <= 1)
    {
      if (buflen)
        *buf = '\0';
      return ERANGE;
    }
  *buf = '\0';

   
  {
    char const *msg = strerror_override (errnum);

    if (msg)
      return safe_copy (buf, buflen, msg);
  }

  {
    int ret;
    int saved_errno = errno;

#if STRERROR_R_CHAR_P

    {
      ret = 0;

# if HAVE___XPG_STRERROR_R
      ret = __xpg_strerror_r (errnum, buf, buflen);
       
# endif

      if (!*buf)
        {
           
          char stackbuf[80];
          char *errstring = strerror_r (errnum, stackbuf, sizeof stackbuf);
          ret = errstring ? safe_copy (buf, buflen, errstring) : errno;
        }
    }

#elif HAVE_DECL_STRERROR_R

    if (buflen > INT_MAX)
      buflen = INT_MAX;

# ifdef __hpux
     
    {
      char stackbuf[80];

      if (buflen < sizeof stackbuf)
        {
          ret = strerror_r (errnum, stackbuf, sizeof stackbuf);
          if (ret == 0)
            ret = safe_copy (buf, buflen, stackbuf);
        }
      else
        ret = strerror_r (errnum, buf, buflen);
    }
# else
    ret = strerror_r (errnum, buf, buflen);

     
#  if !defined __HAIKU__
    if (ret < 0)
      ret = errno;
#  endif
# endif

# if defined _AIX || defined __HAIKU__
     
    if (!ret && strlen (buf) == buflen - 1)
      {
        char stackbuf[STACKBUF_LEN];
        size_t len;
        strerror_r (errnum, stackbuf, sizeof stackbuf);
        len = strlen (stackbuf);
         
        if (len + 1 == sizeof stackbuf)
          abort ();
        if (buflen <= len)
          ret = ERANGE;
      }
# else
     
    if (ret == ERANGE && strlen (buf) < buflen - 1)
      {
        char stackbuf[STACKBUF_LEN];

         
        if (strerror_r (errnum, stackbuf, sizeof stackbuf) == ERANGE)
          abort ();
        safe_copy (buf, buflen, stackbuf);
      }
# endif

#else  

     

# if defined __NetBSD__ || defined __hpux || (defined _WIN32 && !defined __CYGWIN__) || defined __CYGWIN__  

     
    if (errnum >= 0 && errnum < sys_nerr)
      {
#  if HAVE_CATGETS && (defined __NetBSD__ || defined __hpux)
#   if defined __NetBSD__
        nl_catd catd = catopen ("libc", NL_CAT_LOCALE);
        const char *errmsg =
          (catd != (nl_catd)-1
           ? catgets (catd, 1, errnum, sys_errlist[errnum])
           : sys_errlist[errnum]);
#   endif
#   if defined __hpux
        nl_catd catd = catopen ("perror", NL_CAT_LOCALE);
        const char *errmsg =
          (catd != (nl_catd)-1
           ? catgets (catd, 1, 1 + errnum, sys_errlist[errnum])
           : sys_errlist[errnum]);
#   endif
#  else
        const char *errmsg = sys_errlist[errnum];
#  endif
        if (errmsg == NULL || *errmsg == '\0')
          ret = EINVAL;
        else
          ret = safe_copy (buf, buflen, errmsg);
#  if HAVE_CATGETS && (defined __NetBSD__ || defined __hpux)
        if (catd != (nl_catd)-1)
          catclose (catd);
#  endif
      }
    else
      ret = EINVAL;

# elif defined __sgi || (defined __sun && !defined _LP64)  

     
    if (errnum >= 0 && errnum < sys_nerr)
      {
        char *errmsg = strerror (errnum);

        if (errmsg == NULL || *errmsg == '\0')
          ret = EINVAL;
        else
          ret = safe_copy (buf, buflen, errmsg);
      }
    else
      ret = EINVAL;

# else

    gl_lock_lock (strerror_lock);

    {
      char *errmsg = strerror (errnum);

       
      if (errmsg == NULL || *errmsg == '\0')
        ret = EINVAL;
      else
        ret = safe_copy (buf, buflen, errmsg);
    }

    gl_lock_unlock (strerror_lock);

# endif

#endif

#if defined _WIN32 && !defined __CYGWIN__
     
    if (ret == EINVAL)
      {
        const char *errmsg;

        switch (errnum)
          {
          case 100  :
            errmsg = "Address already in use";
            break;
          case 101  :
            errmsg = "Cannot assign requested address";
            break;
          case 102  :
            errmsg = "Address family not supported by protocol";
            break;
          case 103  :
            errmsg = "Operation already in progress";
            break;
          case 105  :
            errmsg = "Operation canceled";
            break;
          case 106  :
            errmsg = "Software caused connection abort";
            break;
          case 107  :
            errmsg = "Connection refused";
            break;
          case 108  :
            errmsg = "Connection reset by peer";
            break;
          case 109  :
            errmsg = "Destination address required";
            break;
          case 110  :
            errmsg = "No route to host";
            break;
          case 112  :
            errmsg = "Operation now in progress";
            break;
          case 113  :
            errmsg = "Transport endpoint is already connected";
            break;
          case 114  :
            errmsg = "Too many levels of symbolic links";
            break;
          case 115  :
            errmsg = "Message too long";
            break;
          case 116  :
            errmsg = "Network is down";
            break;
          case 117  :
            errmsg = "Network dropped connection on reset";
            break;
          case 118  :
            errmsg = "Network is unreachable";
            break;
          case 119  :
            errmsg = "No buffer space available";
            break;
          case 123  :
            errmsg = "Protocol not available";
            break;
          case 126  :
            errmsg = "Transport endpoint is not connected";
            break;
          case 128  :
            errmsg = "Socket operation on non-socket";
            break;
          case 129  :
            errmsg = "Not supported";
            break;
          case 130  :
            errmsg = "Operation not supported";
            break;
          case 132  :
            errmsg = "Value too large for defined data type";
            break;
          case 133  :
            errmsg = "Owner died";
            break;
          case 134  :
            errmsg = "Protocol error";
            break;
          case 135  :
            errmsg = "Protocol not supported";
            break;
          case 136  :
            errmsg = "Protocol wrong type for socket";
            break;
          case 138  :
            errmsg = "Connection timed out";
            break;
          case 140  :
            errmsg = "Operation would block";
            break;
          default:
            errmsg = NULL;
            break;
          }
        if (errmsg != NULL)
          ret = safe_copy (buf, buflen, errmsg);
      }
#endif

    if (ret == EINVAL && !*buf)
      {
#if defined __HAIKU__
         
        snprintf (buf, buflen, "Unknown Application Error (%d)", errnum);
#else
        snprintf (buf, buflen, "Unknown error %d", errnum);
#endif
      }

    errno = saved_errno;
    return ret;
  }
}
