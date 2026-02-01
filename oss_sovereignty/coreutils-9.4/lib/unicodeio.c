 

#include <config.h>

 
#include "unicodeio.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#if HAVE_ICONV
# include <iconv.h>
#endif

#include <error.h>

#include "gettext.h"
#define _(msgid) gettext (msgid)
#define N_(msgid) msgid

#include "localcharset.h"
#include "unistr.h"

 

 
#define UTF8_NAME "UTF-8"

 
long
unicode_to_mb (unsigned int code,
               long (*success) (const char *buf, size_t buflen,
                                void *callback_arg),
               long (*failure) (unsigned int code, const char *msg,
                                void *callback_arg),
               void *callback_arg)
{
  static int initialized;
  static int is_utf8;
#if HAVE_ICONV
  static iconv_t utf8_to_local;
#endif

  char inbuf[6];
  int count;

  if (!initialized)
    {
      const char *charset = locale_charset ();

      is_utf8 = !strcmp (charset, UTF8_NAME);
#if HAVE_ICONV
      if (!is_utf8)
        {
          utf8_to_local = iconv_open (charset, UTF8_NAME);
          if (utf8_to_local == (iconv_t)(-1))
             
            utf8_to_local = iconv_open ("ASCII", UTF8_NAME);
        }
#endif
      initialized = 1;
    }

   
  if (!is_utf8)
    {
#if HAVE_ICONV
      if (utf8_to_local == (iconv_t)(-1))
        return failure (code, N_("iconv function not usable"), callback_arg);
#else
      return failure (code, N_("iconv function not available"), callback_arg);
#endif
    }

   
  count = u8_uctomb ((unsigned char *) inbuf, code, sizeof (inbuf));
  if (count < 0)
    return failure (code, N_("character out of range"), callback_arg);

#if HAVE_ICONV
  if (!is_utf8)
    {
      char outbuf[25];
      const char *inptr;
      size_t inbytesleft;
      char *outptr;
      size_t outbytesleft;
      size_t res;

      inptr = inbuf;
      inbytesleft = count;
      outptr = outbuf;
      outbytesleft = sizeof (outbuf);

       
      res = iconv (utf8_to_local,
                   (ICONV_CONST char **)&inptr, &inbytesleft,
                   &outptr, &outbytesleft);
       
      if (inbytesleft > 0 || res == (size_t)(-1)
           
# if !defined _LIBICONV_VERSION && (defined sgi || defined __sgi)
          || (res > 0 && code != 0 && outptr - outbuf == 1 && *outbuf == '\0')
# endif
           
# if !defined _LIBICONV_VERSION
          || (res > 0 && outptr - outbuf == 1 && *outbuf == '?')
# endif
           
# if !defined _LIBICONV_VERSION && MUSL_LIBC
          || (res > 0 && outptr - outbuf == 1 && *outbuf == '*')
# endif
         )
        return failure (code, NULL, callback_arg);

       
# if defined _LIBICONV_VERSION \
    || !(((__GLIBC__ - 0 == 2 && __GLIBC_MINOR__ - 0 <= 1) \
          && !defined __UCLIBC__) \
         || defined __sun)

       
      res = iconv (utf8_to_local, NULL, NULL, &outptr, &outbytesleft);
      if (res == (size_t)(-1))
        return failure (code, NULL, callback_arg);
# endif

      return success (outbuf, outptr - outbuf, callback_arg);
    }
#endif

   
  return success (inbuf, count, callback_arg);
}

 
long
fwrite_success_callback (const char *buf, size_t buflen, void *callback_arg)
{
  FILE *stream = (FILE *) callback_arg;

   
  fwrite (buf, 1, buflen, stream);
  return 0;
}

 
static long
exit_failure_callback (unsigned int code, const char *msg,
                       _GL_UNUSED void *callback_arg)
{
  if (msg == NULL)
    error (1, 0, _("cannot convert U+%04X to local character set"), code);
  else
    error (1, 0, _("cannot convert U+%04X to local character set: %s"), code,
           gettext (msg));
  return -1;
}

 
static long
fallback_failure_callback (unsigned int code,
                           _GL_UNUSED const char *msg,
                           void *callback_arg)
{
  FILE *stream = (FILE *) callback_arg;

  if (code < 0x10000)
    fprintf (stream, "\\u%04X", code);
  else
    fprintf (stream, "\\U%08X", code);
  return -1;
}

 
void
print_unicode_char (FILE *stream, unsigned int code, int exit_on_error)
{
  unicode_to_mb (code, fwrite_success_callback,
                 exit_on_error
                 ? exit_failure_callback
                 : fallback_failure_callback,
                 stream);
}
