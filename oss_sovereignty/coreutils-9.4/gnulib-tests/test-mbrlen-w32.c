 
  {
    char name_with_codepage[1024];

    sprintf (name_with_codepage, "%s.%d", name, codepage);

     
    if (setlocale (LC_ALL, name_with_codepage) == NULL)
      return 77;
  }
# else
   
  {
     
    extern __declspec(dllimport) unsigned int __lc_codepage;

     
    if (setlocale (LC_ALL, name) == NULL)
      return 77;

     
    __lc_codepage = codepage;
    switch (codepage)
      {
      case 1252:
      case 1256:
        MB_CUR_MAX = 1;
        break;
      case 932:
      case 950:
      case 936:
        MB_CUR_MAX = 2;
        break;
      case 54936:
      case 65001:
        MB_CUR_MAX = 4;
        break;
      }

     
    memset (&state, '\0', sizeof (mbstate_t));
    if (mbrlen (" ", 1, &state) == (size_t)(-1))
      return 77;
  }
# endif

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    ret = mbrlen ("x", 0, &state);
     
    ASSERT (ret == (size_t)(-2) || ret == (size_t)(-1) || ret == 0);
    ASSERT (mbsinit (&state));
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    ret = mbrlen ("", 1, &state);
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
           
          buf[0] = c;
          ret = mbrlen (buf, 1, &state);
          ASSERT (ret == 1);
          ASSERT (mbsinit (&state));
          break;
        }
  }

   
  {
    memset (&state, '\0', sizeof (mbstate_t));
    ret = mbrlen (NULL, 5, &state);
    ASSERT (ret == 0);
    ASSERT (mbsinit (&state));
  }

  switch (codepage)
    {
    case 1252:
       
      {
        char input[] = "B\374\337er";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[1] = '\0';

        ret = mbrlen (input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[2] = '\0';

        ret = mbrlen (input + 3, 2, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[3] = '\0';

        ret = mbrlen (input + 4, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
      }
      return 0;

    case 1256:
       
      {
        char input[] = "x\302\341\346y";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[1] = '\0';

        ret = mbrlen (input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[2] = '\0';

        ret = mbrlen (input + 3, 2, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[3] = '\0';

        ret = mbrlen (input + 4, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
      }
      return 0;

    case 65001:
       
      if (strcmp (locale_charset (), "UTF-8") != 0)
        return 77;
      {
        char input[] = "B\303\274\303\237er";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 1, &state);
        ASSERT (ret == (size_t)(-2));
        ASSERT (!mbsinit (&state));
        input[1] = '\0';

        ret = mbrlen (input + 2, 5, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[2] = '\0';

        ret = mbrlen (input + 3, 4, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[3] = '\0';
        input[4] = '\0';

        ret = mbrlen (input + 5, 2, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[5] = '\0';

        ret = mbrlen (input + 6, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\377", 1, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\303\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\343\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\343\300\200", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\343\200\300", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\363\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\363\300\200\200", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\363\200\300", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\363\200\300\200", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\363\200\200\300", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);
      }
      return 0;

    case 932:
       
      {
        char input[] = "<\223\372\226\173\214\352>";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 2, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[1] = '\0';
        input[2] = '\0';

        ret = mbrlen (input + 3, 1, &state);
        ASSERT (ret == (size_t)(-2));
        ASSERT (!mbsinit (&state));
        input[3] = '\0';

        ret = mbrlen (input + 4, 4, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[4] = '\0';

        ret = mbrlen (input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        ret = mbrlen (input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == 2);
      }
      return 0;

    case 950:
       
      {
        char input[] = "<\244\351\245\273\273\171>";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 2, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[1] = '\0';
        input[2] = '\0';

        ret = mbrlen (input + 3, 1, &state);
        ASSERT (ret == (size_t)(-2));
        ASSERT (!mbsinit (&state));
        input[3] = '\0';

        ret = mbrlen (input + 4, 4, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[4] = '\0';

        ret = mbrlen (input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        ret = mbrlen (input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == 2);
      }
      return 0;

    case 936:
       
      {
        char input[] = "<\310\325\261\276\325\132>";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 2, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[1] = '\0';
        input[2] = '\0';

        ret = mbrlen (input + 3, 1, &state);
        ASSERT (ret == (size_t)(-2));
        ASSERT (!mbsinit (&state));
        input[3] = '\0';

        ret = mbrlen (input + 4, 4, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[4] = '\0';

        ret = mbrlen (input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        ret = mbrlen (input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == 2);
      }
      return 0;

    case 54936:
       
      if (strcmp (locale_charset (), "GB18030") != 0)
        return 77;
      {
        char input[] = "B\250\271\201\060\211\070er";  
        memset (&state, '\0', sizeof (mbstate_t));

        ret = mbrlen (input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        ret = mbrlen (input + 1, 1, &state);
        ASSERT (ret == (size_t)(-2));
        ASSERT (!mbsinit (&state));
        input[1] = '\0';

        ret = mbrlen (input + 2, 7, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[2] = '\0';

        ret = mbrlen (input + 3, 6, &state);
        ASSERT (ret == 4);
        ASSERT (mbsinit (&state));
        input[3] = '\0';
        input[4] = '\0';
        input[5] = '\0';
        input[6] = '\0';

        ret = mbrlen (input + 7, 2, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));
        input[7] = '\0';

        ret = mbrlen (input + 8, 1, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\377", 1, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\225\377", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\201\045", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\201\060\377", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\201\060\377\064", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        ret = mbrlen ("\201\060\211\072", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);
      }
      return 0;

    default:
      return 1;
    }
}

int
main (int argc, char *argv[])
{
  int codepage = atoi (argv[argc - 1]);
  int result;
  int i;

  result = 77;
  for (i = 1; i < argc - 1; i++)
    {
      int ret = test_one_locale (argv[i], codepage);

      if (ret != 77)
        result = ret;
    }

  if (result == 77)
    {
      fprintf (stderr, "Skipping test: found no locale with codepage %d\n",
               codepage);
    }
  return result;
}

#else

int
main (int argc, char *argv[])
{
  fputs ("Skipping test: not a native Windows system\n", stderr);
  return 77;
}

#endif
