 
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

  return c32iscntrl (wc);
}

int
main (int argc, char *argv[])
{
  int is;
  char buf[4];

   
  if (setlocale (LC_ALL, "") == NULL)
    return 1;

   
  is = c32iscntrl (WEOF);
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
            case '\t': case '\v': case '\f':
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
           
          is = for_character ("\177", 1);
          ASSERT (is != 0);
           
          is = for_character ("\240", 1);
          ASSERT (is == 0);
        }
        return 0;

      case '2':
         
        {
           
          is = for_character ("\177", 1);
          ASSERT (is != 0);
           
          is = for_character ("\241\241", 2);
          ASSERT (is == 0);
        }
        return 0;

      case '3':
         
        {
           
          is = for_character ("\177", 1);
          ASSERT (is != 0);
           
          is = for_character ("\302\240", 2);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || defined MUSL_LIBC || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined _AIX || defined __sun || defined __CYGWIN__)
           
          is = for_character ("\342\200\256", 3);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\343\200\200", 3);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || defined MUSL_LIBC || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined _AIX || defined __sun || defined __CYGWIN__)
           
          is = for_character ("\357\273\277", 3);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\360\240\200\200", 4);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || defined MUSL_LIBC || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined _AIX || defined __sun || defined __CYGWIN__ || (defined _WIN32 && !defined __CYGWIN__))
           
          is = for_character ("\363\240\200\201", 4);
          ASSERT (is != 0);
        #endif
        }
        return 0;

      case '4':
         
        #if (defined __GLIBC__ && __GLIBC__ == 2 && __GLIBC_MINOR__ >= 13 && __GLIBC_MINOR__ <= 15) || (GL_CHAR32_T_IS_UNICODE && (defined __NetBSD__ || defined __sun))
        fputs ("Skipping test: The GB18030 converter in this system's iconv is broken.\n", stderr);
        return 77;
        #endif
        {
           
          is = for_character ("\177", 1);
          ASSERT (is != 0);
           
          is = for_character ("\201\060\204\062", 4);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          is = for_character ("\201\066\247\061", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\241\241", 2);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __sun)
           
          is = for_character ("\204\061\225\063", 4);
          ASSERT (is != 0);
        #endif
           
          is = for_character ("\225\062\202\066", 4);
          ASSERT (is == 0);
        #if !(defined __GLIBC__ || (defined __APPLE__ && defined __MACH__) || defined __FreeBSD__ || defined __DragonFly__ || defined __NetBSD__ || defined __sun)
           
          is = for_character ("\323\066\225\071", 4);
          ASSERT (is != 0);
        #endif
        }
        return 0;

      }

  return 1;
}
