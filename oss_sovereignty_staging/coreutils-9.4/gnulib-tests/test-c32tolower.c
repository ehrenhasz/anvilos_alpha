 
#define MBCHAR_BUF_SIZE 6
struct multibyte
{
  size_t nbytes;              
  char buf[MBCHAR_BUF_SIZE];  
};

 
static struct multibyte
for_character (const char *s, size_t n)
{
  mbstate_t state;
  char32_t wc;
  size_t ret;
  struct multibyte result;

  memset (&state, '\0', sizeof (mbstate_t));
  wc = (char32_t) 0xBADFACE;
  ret = mbrtoc32 (&wc, s, n, &state);
  ASSERT (ret == n);

  wc = c32tolower (wc);
  ASSERT (wc != WEOF);

  memset (&state, '\0', sizeof (mbstate_t));
  ret = c32rtomb (result.buf, wc, &state);
  ASSERT (ret != 0);
  if (ret == (size_t)(-1))
     
    result.nbytes = 0;
  else
    {
      ASSERT (ret <= MBCHAR_BUF_SIZE);
      result.nbytes = ret;
    }
  return result;
}

int
main (int argc, char *argv[])
{
  wint_t wc;
  struct multibyte mb;
  char buf[4];

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  wc = c32tolower (WEOF);
  ASSERT (wc == WEOF);

   
#if defined __NetBSD__
   
          buf[0] = (unsigned char) c;
          mb = for_character (buf, 1);
          switch (c)
            {
            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'I': case 'J':
            case 'K': case 'L': case 'M': case 'N': case 'O':
            case 'P': case 'Q': case 'R': case 'S': case 'T':
            case 'U': case 'V': case 'W': case 'X': case 'Y':
            case 'Z':
              ASSERT (mb.nbytes == 1);
              ASSERT ((unsigned char) mb.buf[0] == (unsigned char) c - 'A' + 'a');
              break;
            default:
              ASSERT (mb.nbytes == 1);
              ASSERT ((unsigned char) mb.buf[0] == c);
              break;
            }
          break;
        }
  }

  if (argc > 1)
    switch (argv[1][0])
      {
      case '0':
         
        return 0;

      case '1':
         
        {
           
          mb = for_character ("\262", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\262", 1) == 0);
           
          mb = for_character ("\265", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\265", 1) == 0);
           
          mb = for_character ("\311", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\351", 1) == 0);
           
          mb = for_character ("\337", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\337", 1) == 0);
           
          mb = for_character ("\351", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\351", 1) == 0);
           
          mb = for_character ("\377", 1);
          ASSERT (mb.nbytes == 1);
          ASSERT (memcmp (mb.buf, "\377", 1) == 0);
        }
        return 0;

      case '2':
         
        {
        #if !((defined __APPLE__ && defined __MACH__) || defined __DragonFly__)
           
          mb = for_character ("\217\252\261", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\253\261", 3) == 0);
        #endif
           
          mb = for_character ("\217\251\316", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\251\316", 3) == 0);
           
          mb = for_character ("\217\253\261", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\253\261", 3) == 0);
           
          mb = for_character ("\217\253\363", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\253\363", 3) == 0);
        #if !((defined __APPLE__ && defined __MACH__) || defined __DragonFly__)
           
          mb = for_character ("\217\251\250", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\251\310", 3) == 0);
        #endif
           
          mb = for_character ("\217\251\310", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\217\251\310", 3) == 0);
        #if !defined __DragonFly__
           
          mb = for_character ("\247\273", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\247\353", 2) == 0);
        #endif
           
          mb = for_character ("\247\353", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\247\353", 2) == 0);
           
          mb = for_character ("\244\323", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\244\323", 2) == 0);
        #if !defined __DragonFly__
           
          mb = for_character ("\243\307", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\243\347", 2) == 0);
        #endif
           
          mb = for_character ("\243\347", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\243\347", 2) == 0);
        }
        return 0;

      case '3':
         
        {
           
          mb = for_character ("\302\262", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\302\262", 2) == 0);
           
          mb = for_character ("\302\265", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\302\265", 2) == 0);
        #if !(defined _WIN32 && !defined __CYGWIN__)
           
          mb = for_character ("\303\211", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\303\251", 2) == 0);
        #endif
           
          mb = for_character ("\303\237", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\303\237", 2) == 0);
           
          mb = for_character ("\303\251", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\303\251", 2) == 0);
           
          mb = for_character ("\303\277", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\303\277", 2) == 0);
           
          mb = for_character ("\305\201", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\305\202", 2) == 0);
           
          mb = for_character ("\305\202", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\305\202", 2) == 0);
           
          mb = for_character ("\320\251", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\321\211", 2) == 0);
           
          mb = for_character ("\321\211", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\321\211", 2) == 0);
           
          mb = for_character ("\327\225", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\327\225", 2) == 0);
           
          mb = for_character ("\343\201\263", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\343\201\263", 3) == 0);
           
          mb = for_character ("\343\205\242", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\343\205\242", 3) == 0);
           
          mb = for_character ("\357\274\247", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\357\275\207", 3) == 0);
           
          mb = for_character ("\357\275\207", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\357\275\207", 3) == 0);
           
          mb = for_character ("\357\277\233", 3);
          ASSERT (mb.nbytes == 3);
          ASSERT (memcmp (mb.buf, "\357\277\233", 3) == 0);
        #if !(defined __DragonFly__ || defined __sun)
           
          mb = for_character ("\360\220\220\231", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\360\220\221\201", 4) == 0);
        #endif
           
          mb = for_character ("\360\220\221\201", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\360\220\221\201", 4) == 0);
           
          mb = for_character ("\363\240\201\201", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\363\240\201\201", 4) == 0);
           
          mb = for_character ("\363\240\201\241", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\363\240\201\241", 4) == 0);
        }
        return 0;

      case '4':
         
        #if (defined __GLIBC__ && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 13 && __GLIBC_MINOR__ <= 15) || (GL_CHAR32_T_IS_UNICODE && (defined __NetBSD__ || defined __sun))
        fputs ("Skipping test: The GB18030 converter in this system's iconv is broken.\n", stderr);
        return 77;
        #endif
        {
           
          mb = for_character ("\201\060\205\065", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\205\065", 4) == 0);
           
          mb = for_character ("\201\060\205\070", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\205\070", 4) == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          mb = for_character ("\201\060\207\067", 4);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\250\246", 2) == 0);
        #endif
           
          mb = for_character ("\201\060\211\070", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\211\070", 4) == 0);
           
          mb = for_character ("\250\246", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\250\246", 2) == 0);
           
          mb = for_character ("\201\060\213\067", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\213\067", 4) == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          mb = for_character ("\201\060\221\071", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\222\060", 4) == 0);
        #endif
           
          mb = for_character ("\201\060\222\060", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\222\060", 4) == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__)
           
          mb = for_character ("\247\273", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\247\353", 2) == 0);
        #endif
           
          mb = for_character ("\247\353", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\247\353", 2) == 0);
           
          mb = for_character ("\201\060\371\067", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\060\371\067", 4) == 0);
           
          mb = for_character ("\244\323", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\244\323", 2) == 0);
           
          mb = for_character ("\201\071\256\062", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\201\071\256\062", 4) == 0);
        #if !defined __DragonFly__
           
          mb = for_character ("\243\307", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\243\347", 2) == 0);
        #endif
           
          mb = for_character ("\243\347", 2);
          ASSERT (mb.nbytes == 2);
          ASSERT (memcmp (mb.buf, "\243\347", 2) == 0);
           
          mb = for_character ("\204\061\241\071", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\204\061\241\071", 4) == 0);
        #if !((defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __sun)
           
          mb = for_character ("\220\060\351\071", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\220\060\355\071", 4) == 0);
        #endif
           
          mb = for_character ("\220\060\355\071", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\220\060\355\071", 4) == 0);
           
          mb = for_character ("\323\066\234\063", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\323\066\234\063", 4) == 0);
           
          mb = for_character ("\323\066\237\065", 4);
          ASSERT (mb.nbytes == 4);
          ASSERT (memcmp (mb.buf, "\323\066\237\065", 4) == 0);
        }
        return 0;

      }

  return 1;
}
