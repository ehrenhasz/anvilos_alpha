 
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
    if (mbrtoc32 (&wc, " ", 1, &state) == (size_t)(-1))
      return 77;
  }
# endif

   
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

  switch (codepage)
    {
    case 1252:
       
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
        ASSERT (wc == 0x00FC);  
        ASSERT (mbsinit (&state));
        input[1] = '\0';

         
        ret = mbrtoc32 (NULL, input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (c32tob (wc) == (unsigned char) '\337');
        ASSERT (wc == 0x00DF);  
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

    case 1256:
       
      {
        char input[] = "x\302\341\346y";  
        memset (&state, '\0', sizeof (mbstate_t));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input, 1, &state);
        ASSERT (ret == 1);
        ASSERT (wc == 'x');
        ASSERT (mbsinit (&state));
        input[0] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 1, 1, &state);
        ASSERT (ret == 1);
        ASSERT (c32tob (wc) == (unsigned char) '\302');
        ASSERT (wc == 0x0622);  
        ASSERT (mbsinit (&state));
        input[1] = '\0';

         
        ret = mbrtoc32 (NULL, input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 2, 3, &state);
        ASSERT (ret == 1);
        ASSERT (c32tob (wc) == (unsigned char) '\341');
        ASSERT (wc == 0x0644);  
        ASSERT (mbsinit (&state));
        input[2] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 3, 2, &state);
        ASSERT (ret == 1);
        ASSERT (c32tob (wc) == (unsigned char) '\346');
        ASSERT (wc == 0x0648);  
        ASSERT (mbsinit (&state));
        input[3] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 4, 1, &state);
        ASSERT (ret == 1);
        ASSERT (wc == 'y');
        ASSERT (mbsinit (&state));
      }
      return 0;

    case 65001:
       
      if (strcmp (locale_charset (), "UTF-8") != 0)
        return 77;
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

         
        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\377", 1, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\303\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\343\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\343\300\200", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\343\200\300", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\363\300", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\363\300\200\200", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\363\200\300", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\363\200\300\200", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\363\200\200\300", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);
      }
      return 0;

    case 932:
       
      {
        char input[] = "<\223\372\226\173\214\352>";  
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
        ASSERT (wc == 0x65E5);  
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
        ASSERT (wc == 0x672C);  
        ASSERT (mbsinit (&state));
        input[4] = '\0';

         
        ret = mbrtoc32 (NULL, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (c32tob (wc) == EOF);
        ASSERT (wc == 0x8A9E);  
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (wc == '>');
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || (ret == 2 && wc == 0x30FB));
      }
      return 0;

    case 950:
       
      {
        char input[] = "<\244\351\245\273\273\171>";  
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
        ASSERT (wc == 0x65E5);  
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
        ASSERT (wc == 0x672C);  
        ASSERT (mbsinit (&state));
        input[4] = '\0';

         
        ret = mbrtoc32 (NULL, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (c32tob (wc) == EOF);
        ASSERT (wc == 0x8A9E);  
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (wc == '>');
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || (ret == 2 && wc == '?'));
      }
      return 0;

    case 936:
       
      {
        char input[] = "<\310\325\261\276\325\132>";  
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
        ASSERT (wc == 0x65E5);  
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
        ASSERT (wc == 0x672C);  
        ASSERT (mbsinit (&state));
        input[4] = '\0';

         
        ret = mbrtoc32 (NULL, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 5, 3, &state);
        ASSERT (ret == 2);
        ASSERT (c32tob (wc) == EOF);
        ASSERT (wc == 0x8A9E);  
        ASSERT (mbsinit (&state));
        input[5] = '\0';
        input[6] = '\0';

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 7, 1, &state);
        ASSERT (ret == 1);
        ASSERT (wc == '>');
        ASSERT (mbsinit (&state));

         
        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\377", 1, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || ret == (size_t)-2);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\225\377", 2, &state);  
        ASSERT ((ret == (size_t)-1 && errno == EILSEQ) || (ret == 2 && wc == '?'));
      }
      return 0;

    case 54936:
       
      if (strcmp (locale_charset (), "GB18030") != 0)
        return 77;
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
        ASSERT (wc == 0x00FC);  
        ASSERT (mbsinit (&state));
        input[2] = '\0';

         
        ret = mbrtoc32 (NULL, input + 3, 8, &state);
        ASSERT (ret == 4);
        ASSERT (mbsinit (&state));

        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, input + 3, 8, &state);
        ASSERT (ret == 4);
        ASSERT (c32tob (wc) == EOF);
        ASSERT (wc == 0x00DF);  
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
        ASSERT (wc == 0x1F60B);  
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

         
        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\377", 1, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\225\377", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\201\045", 2, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\201\060\377", 3, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\201\060\377\064", 4, &state);  
        ASSERT (ret == (size_t)-1);
        ASSERT (errno == EILSEQ);

        memset (&state, '\0', sizeof (mbstate_t));
        wc = (char32_t) 0xBADFACE;
        ret = mbrtoc32 (&wc, "\201\060\211\072", 4, &state);  
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
