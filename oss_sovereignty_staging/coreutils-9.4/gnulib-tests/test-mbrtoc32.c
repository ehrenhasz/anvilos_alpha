 

#include <config.h>

#include <uchar.h>

#include "signature.h"
SIGNATURE_CHECK (mbrtoc32, size_t,
                 (char32_t *, const char *, size_t, mbstate_t *));

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "macros.h"

int
main (int argc, char *argv[])
{
  mbstate_t state;
  char32_t wc;
  size_t ret;

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (char32_t) 0xBADFACE;
    ret = mbrtoc32 (&wc, "x", 0, &state);
    ASSERT (ret == (size_t)(-2));
    ASSERT (mbsinit (&state));
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (char32_t) 0xBADFACE;
    ret = mbrtoc32 (&wc, "", 1, &state);
    ASSERT (ret == 0);
    ASSERT (wc == 0);
    ASSERT (mbsinit (&state));
    ret = mbrtoc32 (NULL, "", 1, &state);
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

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, buf, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == c);
          ASSERT (mbsinit (&state));

          ret = mbrtoc32 (NULL, buf, 1, &state);
          ASSERT (ret == 1);
          ASSERT (mbsinit (&state));

          break;
        default:
          break;
        }
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    wc = (char32_t) 0xBADFACE;
    ret = mbrtoc32 (&wc, NULL, 5, &state);
    ASSERT (ret == 0);
    ASSERT (wc == (char32_t) 0xBADFACE);
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

                wc = (char32_t) 0xBADFACE;
                ret = mbrtoc32 (&wc, buf, 1, &state);
                 
                ASSERT (ret == 1);
                if (c < 0x80)
                   
                  ASSERT (wc == c);
                else
                   
                  ASSERT (wc == (btoc32 (c) == 0xDF00 + c ? btoc32 (c) : c));
                ASSERT (mbsinit (&state));

                ret = mbrtoc32 (NULL, buf, 1, &state);
                ASSERT (ret == 1);
                ASSERT (mbsinit (&state));
              }
        }
        return 0;

      case '2':
         
        {
          char input[] = "B\374\337er";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'B');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 1, 1, &state);
          ASSERT (ret == 1);
          ASSERT (c32tob (wc) == (unsigned char) '\374');
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x00FC);  
          #endif
          ASSERT (mbsinit (&state));
          input[1] = '\0';

           
          ret = mbrtoc32 (NULL, input + 2, 3, &state);
          ASSERT (ret == 1);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 2, 3, &state);
          ASSERT (ret == 1);
          ASSERT (c32tob (wc) == (unsigned char) '\337');
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x00DF);  
          #endif
          ASSERT (mbsinit (&state));
          input[2] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 3, 2, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'e');
          ASSERT (mbsinit (&state));
          input[3] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 4, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 'r');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '3':
         
        {
          char input[] = "s\303\274\303\237\360\237\230\213!";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 's');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 1, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (char32_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[1] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 2, 7, &state);
          ASSERT (ret == 1);
          ASSERT (c32tob (wc) == EOF);
          ASSERT (wc == 0x00FC);  
          ASSERT (mbsinit (&state));
          input[2] = '\0';

           
          ret = mbrtoc32 (NULL, input + 3, 6, &state);
          ASSERT (ret == 2);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 3, 6, &state);
          ASSERT (ret == 2);
          ASSERT (c32tob (wc) == EOF);
          ASSERT (wc == 0x00DF);  
          ASSERT (mbsinit (&state));
          input[3] = '\0';
          input[4] = '\0';

           
          ret = mbrtoc32 (NULL, input + 5, 4, &state);
          ASSERT (ret == 4);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 5, 4, &state);
          ASSERT (ret == 4);
          ASSERT (c32tob (wc) == EOF);
          ASSERT (wc == 0x1F60B);  
          ASSERT (mbsinit (&state));
          input[5] = '\0';
          input[6] = '\0';
          input[7] = '\0';
          input[8] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 9, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '!');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '4':
         
        {
          char input[] = "<\306\374\313\334\270\354>";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '<');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 1, 2, &state);
          ASSERT (ret == 2);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x65E5);  
          #endif
          ASSERT (mbsinit (&state));
          input[1] = '\0';
          input[2] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 3, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (char32_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[3] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 4, 4, &state);
          ASSERT (ret == 1);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x672C);  
          #endif
          ASSERT (mbsinit (&state));
          input[4] = '\0';

           
          ret = mbrtoc32 (NULL, input + 5, 3, &state);
          ASSERT (ret == 2);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 5, 3, &state);
          ASSERT (ret == 2);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x8A9E);  
          #endif
          ASSERT (mbsinit (&state));
          input[5] = '\0';
          input[6] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 7, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '>');
          ASSERT (mbsinit (&state));
        }
        return 0;

      case '5':
         
        #if (defined __GLIBC__ && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 13 && __GLIBC_MINOR__ <= 15) || (GL_CHAR32_T_IS_UNICODE && (defined __NetBSD__ || defined __sun))
        fputs ("Skipping test: The GB18030 converter in this system's iconv is broken.\n", stderr);
        return 77;
        #endif
        {
          char input[] = "s\250\271\201\060\211\070\224\071\375\067!";  
          memset (&state, '\0', sizeof (mbstate_t));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == 's');
          ASSERT (mbsinit (&state));
          input[0] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 1, 1, &state);
          ASSERT (ret == (size_t)(-2));
          ASSERT (wc == (char32_t) 0xBADFACE);
          ASSERT (!mbsinit (&state));
          input[1] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 2, 9, &state);
          ASSERT (ret == 1);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x00FC);  
          #endif
          ASSERT (mbsinit (&state));
          input[2] = '\0';

           
          ret = mbrtoc32 (NULL, input + 3, 8, &state);
          ASSERT (ret == 4);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 3, 8, &state);
          ASSERT (ret == 4);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x00DF);  
          #endif
          ASSERT (mbsinit (&state));
          input[3] = '\0';
          input[4] = '\0';
          input[5] = '\0';
          input[6] = '\0';

           
          ret = mbrtoc32 (NULL, input + 7, 4, &state);
          ASSERT (ret == 4);
          ASSERT (mbsinit (&state));

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 7, 4, &state);
          ASSERT (ret == 4);
          ASSERT (c32tob (wc) == EOF);
          #if GL_CHAR32_T_IS_UNICODE
          ASSERT (wc == 0x1F60B);  
          #endif
          ASSERT (mbsinit (&state));
          input[7] = '\0';
          input[8] = '\0';
          input[9] = '\0';
          input[10] = '\0';

          wc = (char32_t) 0xBADFACE;
          ret = mbrtoc32 (&wc, input + 11, 1, &state);
          ASSERT (ret == 1);
          ASSERT (wc == '!');
          ASSERT (mbsinit (&state));
        }
        return 0;
      }

  return 1;
}
