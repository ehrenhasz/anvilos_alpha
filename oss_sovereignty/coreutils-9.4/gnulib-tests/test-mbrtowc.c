 

#include <config.h>

#include <wchar.h>

#include "signature.h"
SIGNATURE_CHECK (mbrtowc, size_t, (wchar_t *, char const *, size_t,
                                   mbstate_t *));

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  mbstate_t state;
  wchar_t wc;
  size_t ret;

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (wchar_t) 0xBADFACE;
    ret = mbrtowc (&wc, "x", 0, &state);
    ASSERT (ret == (size_t)(-2));
    ASSERT (mbsinit (&state));
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (wchar_t) 0xBADFACE;
    ret = mbrtowc (&wc, "", 1, &state);
    ASSERT (ret == 0);
    ASSERT (wc == 0);
    ASSERT (mbsinit (&state));
    ret = mbrtowc (NULL, "", 1, &state);
    ASSERT (ret == 0);
    ASSERT (mbsinit (&state));
  }

   
  {
    int c;
    char buf[1];

    memset (&state, '\0', sizeof (mbstate_t));
    for (c = 0; c < 0x100; c++)
      switch (c)
        {
        case '\t': case '\v': case '\f':
        case ' ': case '!': case '"': case '#': case '%':
        case '&': case '\'': case '(': case ')': case '*':
        case '+': case ',': case '-': case '.': case '/':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case ':': case ';': case '<': case '=': case '>':
        case '?':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z':
        case '[': case '\\': case ']': case '^': case '_':
        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z': case '{': case '|': case '}': case '~':
           
          ASSERT (c < 0x80);
           
          buf[0] = c;

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, buf, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == c);
          ASSERT (mbsinit (&state));

          ret = mbrtowc (NULL, buf, 1, &state);
          ASSERT (ret == 1);
          ASSERT (mbsinit (&state));

          break;
        default:
          break;
        }
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (wchar_t) 0xBADFACE;
    ret = mbrtowc (&wc, NULL, 5, &state);
    ASSERT (ret == 0);
    ASSERT (wc == (wchar_t) 0xBADFACE);
    ASSERT (mbsinit (&state));
  }

#ifdef __ANDROID__
   
  if (argc > 1 && strcmp (argv[1], "1") == 0 && MB_CUR_MAX > 1)
    argv[1] = "3";
#endif

  if (argc > 1)
    switch (argv[1][0])
      {
      case '1':
         
        {
          int c;
          char buf[1];

          memset (&state, '\0', sizeof (mbstate_t));
          for (c = 0; c < 0x100; c++)
            if (c != 0)
              {
                 
                buf[0] = c;

                wc = (wchar_t) 0xBADFACE;
                ret = mbrtowc (&wc, buf, 1, &state);
                 
                ASSERT (ret == 1);
                if (c < 0x80)
                   
                  ASSERT (wc == c);
                else
                   
                  ASSERT (wc == (btowc (c) == 0xDF00 + c ? btowc (c) : c));
                ASSERT (mbsinit (&state));

                ret = mbrtowc (NULL, buf, 1, &state);
                ASSERT (ret == 1);
                ASSERT (mbsinit (&state));
              }
        }
        return 0;

      case '2':
         
        {
          char input[] = "B\374\337er";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'B');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 1, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wctob (wc) == (unsigned char) '\374');
          ASSERT (mbsinit (&state));
          input[1] = '\0';

           
          ret = mbrtowc (NULL, input + 2, 3, &state);
          ASSERT (ret == 1);
          ASSERT (mbsinit (&state));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 2, 3, &state);
          ASSERT (ret == 1);
          ASSERT (wctob (wc) == (unsigned char) '\337');
          ASSERT (mbsinit (&state));
          input[2] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 3, 2, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'e');
          ASSERT (mbsinit (&state));
          input[3] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 4, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'r');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '3':
         
        {
          char input[] = "B\303\274\303\237er";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'B');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 1, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (wchar_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[1] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 2, 5, &state);
          ASSERT (ret == 1);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[2] = '\0';

           
          ret = mbrtowc (NULL, input + 3, 4, &state);
          ASSERT (ret == 2);
          ASSERT (mbsinit (&state));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 3, 4, &state);
          ASSERT (ret == 2);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[3] = '\0';
          input[4] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 5, 2, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'e');
          ASSERT (mbsinit (&state));
          input[5] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 6, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'r');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '4':
         
        {
          char input[] = "<\306\374\313\334\270\354>";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '<');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 1, 2, &state);
          ASSERT (ret == 2);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[1] = '\0';
          input[2] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 3, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (wchar_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[3] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 4, 4, &state);
          ASSERT (ret == 1);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[4] = '\0';

           
          ret = mbrtowc (NULL, input + 5, 3, &state);
          ASSERT (ret == 2);
          ASSERT (mbsinit (&state));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 5, 3, &state);
          ASSERT (ret == 2);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[5] = '\0';
          input[6] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 7, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '>');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '5':
         
        {
          char input[] = "B\250\271\201\060\211\070er";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'B');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 1, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (wchar_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[1] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 2, 7, &state);
          ASSERT (ret == 1);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[2] = '\0';

           
          ret = mbrtowc (NULL, input + 3, 6, &state);
          ASSERT (ret == 4);
          ASSERT (mbsinit (&state));

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 3, 6, &state);
          ASSERT (ret == 4);
          ASSERT (wctob (wc) == EOF);
          ASSERT (mbsinit (&state));
          input[3] = '\0';
          input[4] = '\0';
          input[5] = '\0';
          input[6] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 7, 2, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'e');
          ASSERT (mbsinit (&state));
          input[7] = '\0';

          wc = (wchar_t) 0xBADFACE;
          ret = mbrtowc (&wc, input + 8, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'r');
          ASSERT (mbsinit (&state));
        }
        return 0;
      }

  return 1;
}
