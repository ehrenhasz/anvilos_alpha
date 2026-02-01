 
static int
for_character (const char *s, size_t n)
{
  mbstate_t state;
  char32_t wc;
  size_t ret;

  memset (&state, '\0', sizeof (mbstate_t));
  wc = (char32_t) 0xBADFACE;
  ret = mbrtoc32 (&wc, s, n, &state);
  ASSERT (ret == n);

  return c32isupper (wc);
}

int
main (int argc, char *argv[])
{
  int is;
  char buf[4];

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  is = c32isupper (WEOF);
  ASSERT (is == 0);

   
  {
    int c;

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
           
          buf[0] = (unsigned char) c;
          is = for_character (buf, 1);
          switch (c)
            {
            case 'A': case 'B': case 'C': case 'D': case 'E':
            case 'F': case 'G': case 'H': case 'I': case 'J':
            case 'K': case 'L': case 'M': case 'N': case 'O':
            case 'P': case 'Q': case 'R': case 'S': case 'T':
            case 'U': case 'V': case 'W': case 'X': case 'Y':
            case 'Z':
              ASSERT (is != 0);
              break;
            default:
              ASSERT (is == 0);
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
           
          is = for_character ("\262", 1);
          ASSERT (is == 0);
           
          is = for_character ("\265", 1);
          ASSERT (is == 0);
           
          is = for_character ("\311", 1);
          ASSERT (is != 0);
        #if !defined __hpux
           
          is = for_character ("\337", 1);
          ASSERT (is == 0);
        #endif
           
          is = for_character ("\351", 1);
          ASSERT (is == 0);
           
          is = for_character ("\377", 1);
          ASSERT (is == 0);
        }
        return 0;

      case '2':
         
        {
        #if !((defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__)
           
          is = for_character ("\217\252\261", 3);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\217\251\316", 3);
          ASSERT (is == 0);
           
          is = for_character ("\217\253\261", 3);
          ASSERT (is == 0);
           
          is = for_character ("\217\253\363", 3);
          ASSERT (is == 0);
        #if !((defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__)
           
          is = for_character ("\217\251\250", 3);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\217\251\310", 3);
          ASSERT (is == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__)
           
          is = for_character ("\247\273", 2);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\247\353", 2);
          ASSERT (is == 0);
           
          is = for_character ("\244\323", 2);
          ASSERT (is == 0);
        #if !defined __DragonFly__
           
          is = for_character ("\243\307", 2);
          ASSERT (is != 0);
        #endif
        }
        return 0;

      case '3':
         
        {
           
          is = for_character ("\302\262", 2);
          ASSERT (is == 0);
           
          is = for_character ("\302\265", 2);
          ASSERT (is == 0);
           
          is = for_character ("\303\211", 2);
          ASSERT (is != 0);
           
          is = for_character ("\303\237", 2);
          ASSERT (is == 0);
           
          is = for_character ("\303\251", 2);
          ASSERT (is == 0);
           
          is = for_character ("\303\277", 2);
          ASSERT (is == 0);
           
          is = for_character ("\305\201", 2);
          ASSERT (is != 0);
           
          is = for_character ("\305\202", 2);
          ASSERT (is == 0);
           
          is = for_character ("\320\251", 2);
          ASSERT (is != 0);
           
          is = for_character ("\321\211", 2);
          ASSERT (is == 0);
           
          is = for_character ("\327\225", 2);
          ASSERT (is == 0);
           
          is = for_character ("\343\201\263", 3);
          ASSERT (is == 0);
           
          is = for_character ("\343\205\242", 3);
          ASSERT (is == 0);
           
          is = for_character ("\357\274\247", 3);
          ASSERT (is != 0);
           
          is = for_character ("\357\277\233", 3);
          ASSERT (is == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          is = for_character ("\360\220\220\231", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\360\220\221\201", 4);
          ASSERT (is == 0);
           
          is = for_character ("\363\240\201\201", 4);
          ASSERT (is == 0);
           
          is = for_character ("\363\240\201\241", 4);
          ASSERT (is == 0);
        }
        return 0;

      case '4':
         
        #if (defined __GLIBC__ && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 13 && __GLIBC_MINOR__ <= 15) || (GL_CHAR32_T_IS_UNICODE && (defined __NetBSD__ || defined __sun))
        fputs ("Skipping test: The GB18030 converter in this system's iconv is broken.\n", stderr);
        return 77;
        #endif
        {
           
          is = for_character ("\201\060\205\065", 4);
          ASSERT (is == 0);
           
          is = for_character ("\201\060\205\070", 4);
          ASSERT (is == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          is = for_character ("\201\060\207\067", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\201\060\211\070", 4);
          ASSERT (is == 0);
           
          is = for_character ("\250\246", 2);
          ASSERT (is == 0);
           
          is = for_character ("\201\060\213\067", 4);
          ASSERT (is == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          is = for_character ("\201\060\221\071", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\201\060\222\060", 4);
          ASSERT (is == 0);
        #if !(defined __FreeBSD__ || defined __DragonFly__)
           
          is = for_character ("\247\273", 2);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\247\353", 2);
          ASSERT (is == 0);
           
          is = for_character ("\201\060\371\067", 4);
          ASSERT (is == 0);
           
          is = for_character ("\244\323", 2);
          ASSERT (is == 0);
           
          is = for_character ("\201\071\256\062", 4);
          ASSERT (is == 0);
        #if !defined __DragonFly__
           
          is = for_character ("\243\307", 2);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\204\061\241\071", 4);
          ASSERT (is == 0);
        #if !((defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __sun)
           
          is = for_character ("\220\060\351\071", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\220\060\355\071", 4);
          ASSERT (is == 0);
           
          is = for_character ("\323\066\234\063", 4);
          ASSERT (is == 0);
           
          is = for_character ("\323\066\237\065", 4);
          ASSERT (is == 0);
        }
        return 0;

      }

  return 1;
}
