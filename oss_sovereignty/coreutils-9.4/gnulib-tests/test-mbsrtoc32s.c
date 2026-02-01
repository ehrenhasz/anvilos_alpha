 

#include <config.h>

#include <uchar.h>

#include "signature.h"
SIGNATURE_CHECK (mbsrtoc32s, size_t,
                 (char32_t *, const char **, size_t, mbstate_t *));

#include <locale.h>
#include <stdio.h>
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
    const char *src;

    memset (&state, '\0', sizeof (mbstate_t));

    src = "";
    ret = mbsrtoc32s (NULL, &src, 0, &state);
    ASSERT (ret == 0);
    ASSERT (mbsinit (&state));

    src = "";
    ret = mbsrtoc32s (NULL, &src, 1, &state);
    ASSERT (ret == 0);
    ASSERT (mbsinit (&state));

    wc = (char32_t) 0xBADFACE;
    src = "";
    ret = mbsrtoc32s (&wc, &src, 0, &state);
    ASSERT (ret == 0);
    ASSERT (wc == (char32_t) 0xBADFACE);
    ASSERT (mbsinit (&state));

    wc = (char32_t) 0xBADFACE;
    src = "";
    ret = mbsrtoc32s (&wc, &src, 1, &state);
    ASSERT (ret == 0);
    ASSERT (wc == 0);
    ASSERT (mbsinit (&state));
  }

#ifdef __ANDROID__
   
  if (argc > 1 && strcmp (argv[1], "1") == 0 && MB_CUR_MAX > 1)
    argv[1] = "3";
#endif

  if (argc > 1)
    {
      int unlimited;

      for (unlimited = 0; unlimited < 2; unlimited++)
        {
          #define BUFSIZE 10
          char32_t buf[BUFSIZE];
          const char *src;
          mbstate_t temp_state;

          {
            size_t i;
            for (i = 0; i < BUFSIZE; i++)
              buf[i] = (char32_t) 0xBADFACE;
          }

          switch (argv[1][0])
            {
            case '1':
               
              {
                char input[] = "n/a";
                memset (&state, '\0', sizeof (mbstate_t));

                src = input;
                temp_state = state;
                ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 1, &temp_state);
                ASSERT (ret == 3);
                ASSERT (src == input);
                ASSERT (mbsinit (&state));

                src = input;
                ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 1, &state);
                ASSERT (ret == (unlimited ? 3 : 1));
                ASSERT (src == (unlimited ? NULL : input + 1));
                ASSERT (buf[0] == 'n');
                if (unlimited)
                  {
                    ASSERT (buf[1] == '/');
                    ASSERT (buf[2] == 'a');
                    ASSERT (buf[3] == 0);
                    ASSERT (buf[4] == (char32_t) 0xBADFACE);
                  }
                else
                  ASSERT (buf[1] == (char32_t) 0xBADFACE);
                ASSERT (mbsinit (&state));
              }
              {
                int c;
                char input[2];

                memset (&state, '\0', sizeof (mbstate_t));
                for (c = 0; c < 0x100; c++)
                  if (c != 0)
                    {
                       
                      input[0] = c;
                      input[1] = '\0';

                      src = input;
                      ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 1, &state);
                      ASSERT (ret == 1);
                      ASSERT (src == input);
                      ASSERT (mbsinit (&state));

                      buf[0] = buf[1] = (char32_t) 0xBADFACE;
                      src = input;
                      ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 1, &state);
                       
                      ASSERT (ret == 1);
                      ASSERT (src == (unlimited ? NULL : input + 1));
                      if (c < 0x80)
                         
                        ASSERT (buf[0] == c);
                      else
                         
                        ASSERT (buf[0] == (btoc32 (c) == 0xDF00 + c ? btoc32 (c) : c));
                      ASSERT (mbsinit (&state));
                    }
              }
              break;

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
                ASSERT (mbsinit (&state));
                input[1] = '\0';

                src = input + 2;
                temp_state = state;
                ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 1, &temp_state);
                ASSERT (ret == 3);
                ASSERT (src == input + 2);
                ASSERT (mbsinit (&state));

                src = input + 2;
                ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 1, &state);
                ASSERT (ret == (unlimited ? 3 : 1));
                ASSERT (src == (unlimited ? NULL : input + 3));
                ASSERT (c32tob (buf[0]) == (unsigned char) '\337');
                if (unlimited)
                  {
                    ASSERT (buf[1] == 'e');
                    ASSERT (buf[2] == 'r');
                    ASSERT (buf[3] == 0);
                    ASSERT (buf[4] == (char32_t) 0xBADFACE);
                  }
                else
                  ASSERT (buf[1] == (char32_t) 0xBADFACE);
                ASSERT (mbsinit (&state));
              }
              break;

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

                src = input + 2;
                temp_state = state;
                ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 2, &temp_state);
                ASSERT (ret == 4);
                ASSERT (src == input + 2);
                ASSERT (!mbsinit (&state));

                src = input + 2;
                ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 2, &state);
                ASSERT (ret == (unlimited ? 4 : 2));
                ASSERT (src == (unlimited ? NULL : input + 5));
                ASSERT (c32tob (buf[0]) == EOF);
                ASSERT (c32tob (buf[1]) == EOF);
                if (unlimited)
                  {
                    ASSERT (buf[2] == 0x1F60B);  
                    ASSERT (buf[3] == '!');
                    ASSERT (buf[4] == 0);
                    ASSERT (buf[5] == (char32_t) 0xBADFACE);
                  }
                else
                  ASSERT (buf[2] == (char32_t) 0xBADFACE);
                ASSERT (mbsinit (&state));
              }
              break;

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
                ASSERT (mbsinit (&state));
                input[1] = '\0';
                input[2] = '\0';

                wc = (char32_t) 0xBADFACE;
                ret = mbrtoc32 (&wc, input + 3, 1, &state);
                ASSERT (ret == (size_t)(-2));
                ASSERT (wc == (char32_t) 0xBADFACE);
                ASSERT (!mbsinit (&state));
                input[3] = '\0';

                src = input + 4;
                temp_state = state;
                ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 2, &temp_state);
                ASSERT (ret == 3);
                ASSERT (src == input + 4);
                ASSERT (!mbsinit (&state));

                src = input + 4;
                ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 2, &state);
                ASSERT (ret == (unlimited ? 3 : 2));
                ASSERT (src == (unlimited ? NULL : input + 7));
                ASSERT (c32tob (buf[0]) == EOF);
                ASSERT (c32tob (buf[1]) == EOF);
                if (unlimited)
                  {
                    ASSERT (buf[2] == '>');
                    ASSERT (buf[3] == 0);
                    ASSERT (buf[4] == (char32_t) 0xBADFACE);
                  }
                else
                  ASSERT (buf[2] == (char32_t) 0xBADFACE);
                ASSERT (mbsinit (&state));
              }
              break;

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

                src = input + 2;
                temp_state = state;
                ret = mbsrtoc32s (NULL, &src, unlimited ? BUFSIZE : 2, &temp_state);
                ASSERT (ret == 4);
                ASSERT (src == input + 2);
                ASSERT (!mbsinit (&state));

                src = input + 2;
                ret = mbsrtoc32s (buf, &src, unlimited ? BUFSIZE : 2, &state);
                ASSERT (ret == (unlimited ? 4 : 2));
                ASSERT (src == (unlimited ? NULL : input + 7));
                ASSERT (c32tob (buf[0]) == EOF);
                ASSERT (c32tob (buf[1]) == EOF);
                if (unlimited)
                  {
                    ASSERT (c32tob (buf[2]) == EOF);
                    ASSERT (buf[3] == '!');
                    ASSERT (buf[4] == 0);
                    ASSERT (buf[5] == (char32_t) 0xBADFACE);
                  }
                else
                  ASSERT (buf[2] == (char32_t) 0xBADFACE);
                ASSERT (mbsinit (&state));
              }
              break;

            default:
              return 1;
            }
        }

      return 0;
    }

  return 1;
}
