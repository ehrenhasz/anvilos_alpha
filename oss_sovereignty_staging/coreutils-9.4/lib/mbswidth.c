 

#include <config.h>

 
#include "mbswidth.h"

 
#include <stdlib.h>

#include <string.h>

 
#include <ctype.h>

 
#include <wchar.h>

 
#include <uchar.h>

 
#include <limits.h>

 
int
mbswidth (const char *string, int flags)
{
  return mbsnwidth (string, strlen (string), flags);
}

 
int
mbsnwidth (const char *string, size_t nbytes, int flags)
{
  const char *p = string;
  const char *plimit = p + nbytes;
  int width;

  width = 0;
  if (MB_CUR_MAX > 1)
    {
      while (p < plimit)
        switch (*p)
          {
            case ' ': case '!': case '"': case '#': case '$': case '%':
            case '&': case '\'': case '(': case ')': case '*':
            case '+': case ',': case '-': case '.': case '/':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
            case ':': case ';': case '<': case '=': case '>':
            case '?': case '@':
            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'I': case 'J':
            case 'K': case 'L': case 'M': case 'N': case 'O':
            case 'P': case 'Q': case 'R': case 'S': case 'T':
            case 'U': case 'V': case 'W': case 'X': case 'Y':
            case 'Z':
            case '[': case '\\': case ']': case '^': case '_': case '`':
            case 'a': case 'b': case 'c': case 'd': case 'e':
            case 'f': case 'g': case 'h': case 'i': case 'j':
            case 'k': case 'l': case 'm': case 'n': case 'o':
            case 'p': case 'q': case 'r': case 's': case 't':
            case 'u': case 'v': case 'w': case 'x': case 'y':
            case 'z': case '{': case '|': case '}': case '~':
               
              p++;
              width++;
              break;
            default:
               
              {
                mbstate_t mbstate;
                mbszero (&mbstate);
                for (;;)
                  {
                    char32_t wc;
                    size_t bytes;
                    int w;

                    bytes = mbrtoc32 (&wc, p, plimit - p, &mbstate);

                    if (bytes == (size_t) -1)
                       
                      {
                        if (!(flags & MBSW_REJECT_INVALID))
                          {
                            p++;
                            width++;
                            break;
                          }
                        else
                          return -1;
                      }

                    if (bytes == (size_t) -2)
                       
                      {
                        if (!(flags & MBSW_REJECT_INVALID))
                          {
                            p = plimit;
                            width++;
                            break;
                          }
                        else
                          return -1;
                      }

                    if (bytes == 0)
                       
                      bytes = 1;
                    #if !GNULIB_MBRTOC32_REGULAR
                    else if (bytes == (size_t) -3)
                      bytes = 0;
                    #endif

                    w = c32width (wc);
                    if (w >= 0)
                       
                      {
                        if (w > INT_MAX - width)
                          goto overflow;
                        width += w;
                      }
                    else
                       
                      if (!(flags & MBSW_REJECT_UNPRINTABLE))
                        {
                          if (!c32iscntrl (wc))
                            {
                              if (width == INT_MAX)
                                goto overflow;
                              width++;
                            }
                        }
                      else
                        return -1;

                    p += bytes;
                    #if !GNULIB_MBRTOC32_REGULAR
                    if (mbsinit (&mbstate))
                    #endif
                      break;
                  }
              }
              break;
          }
      return width;
    }

  while (p < plimit)
    {
      unsigned char c = (unsigned char) *p++;

      if (isprint (c))
        {
          if (width == INT_MAX)
            goto overflow;
          width++;
        }
      else if (!(flags & MBSW_REJECT_UNPRINTABLE))
        {
          if (!iscntrl (c))
            {
              if (width == INT_MAX)
                goto overflow;
              width++;
            }
        }
      else
        return -1;
    }
  return width;

 overflow:
  return INT_MAX;
}
