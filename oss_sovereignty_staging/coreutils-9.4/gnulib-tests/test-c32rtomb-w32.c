 
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

     
    {
      mbstate_t state;
      wchar_t wc;

      memset (&state, '\0', sizeof (mbstate_t));
      if (mbrtowc (&wc, " ", 1, &state) == (size_t)(-1))
        return 77;
    }
  }
# endif

   
  {
    buf[0] = 'x';
    ret = c32rtomb (buf, 0, NULL);
    ASSERT (ret == 1);
    ASSERT (buf[0] == '\0');
  }

   
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
           
          ret = c32rtomb (buf, btoc32 (c), NULL);
          ASSERT (ret == 1);
          ASSERT (buf[0] == (char) c);
          break;
        }
  }

   
  {
    ret = c32rtomb (NULL, '\0', NULL);
    ASSERT (ret == 1);
    ret = c32rtomb (NULL, btoc32 ('x'), NULL);
    ASSERT (ret == 1);
  }

  switch (codepage)
    {
    case 1252:
       
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00FC, NULL);
        ASSERT (ret == 1);
        ASSERT (memcmp (buf, "\374", 1) == 0);
        ASSERT (buf[1] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00DF, NULL);
        ASSERT (ret == 1);
        ASSERT (memcmp (buf, "\337", 1) == 0);
        ASSERT (buf[1] == 'x');
      }
      return 0;

    case 1256:
       
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x0622, NULL);
        ASSERT (ret == 1);
        ASSERT (memcmp (buf, "\302", 1) == 0);
        ASSERT (buf[1] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x0644, NULL);
        ASSERT (ret == 1);
        ASSERT (memcmp (buf, "\341", 1) == 0);
        ASSERT (buf[1] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x0648, NULL);
        ASSERT (ret == 1);
        ASSERT (memcmp (buf, "\346", 1) == 0);
        ASSERT (buf[1] == 'x');
      }
      return 0;

    case 65001:
       
      if (strcmp (locale_charset (), "UTF-8") != 0)
        return 77;
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00FC, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\303\274", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00DF, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\303\237", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x1F60B, NULL);
        ASSERT (ret == 4);
        ASSERT (memcmp (buf, "\360\237\230\213", 4) == 0);
        ASSERT (buf[4] == 'x');
      }
      return 0;

    case 932:
       
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x65E5, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\223\372", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x672C, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\226\173", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x8A9E, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\214\352", 2) == 0);
        ASSERT (buf[2] == 'x');
      }
      return 0;

    case 950:
       
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x65E5, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\244\351", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x672C, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\245\273", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x8A9E, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\273\171", 2) == 0);
        ASSERT (buf[2] == 'x');
      }
      return 0;

    case 936:
       
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x65E5, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\310\325", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x672C, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\261\276", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x8A9E, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\325\132", 2) == 0);
        ASSERT (buf[2] == 'x');
      }
      return 0;

    case 54936:
       
      if (strcmp (locale_charset (), "GB18030") != 0)
        return 77;
      {
         
        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00FC, NULL);
        ASSERT (ret == 2);
        ASSERT (memcmp (buf, "\250\271", 2) == 0);
        ASSERT (buf[2] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x00DF, NULL);
        ASSERT (ret == 4);
        ASSERT (memcmp (buf, "\201\060\211\070", 4) == 0);
        ASSERT (buf[4] == 'x');

        memset (buf, 'x', 8);
        ret = c32rtomb (buf, 0x1F60B, NULL);
        ASSERT (ret == 4);
        ASSERT (memcmp (buf, "\224\071\375\067", 4) == 0);
        ASSERT (buf[4] == 'x');
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
