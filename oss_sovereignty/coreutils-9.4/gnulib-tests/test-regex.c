 
static int
really_utf8 (void)
{
  return strcmp (locale_charset (), "UTF-8") == 0;
}

 
static struct
{
  const char *pattern;
  const char *string;
  int flags, nmatch;
  regmatch_t rm[5];
} const tests[] = {
   
  { "[^~]*~", "\nx~y", 0, 2, { { 0, 3 }, { -1, -1 } } },
   
  { "a(.*)b", "a b", REG_EXTENDED, 2, { { 0, 3 }, { 1, 2 } } },
  { ".*|\\([KIO]\\)\\([^|]*\\).*|?[KIO]", "10~.~|P|K0|I10|O16|?KSb", 0, 3,
    { { 0, 21 }, { 15, 16 }, { 16, 18 } } },
  { ".*|\\([KIO]\\)\\([^|]*\\).*|?\\1", "10~.~|P|K0|I10|O16|?KSb", 0, 3,
    { { 0, 21 }, { 8, 9 }, { 9, 10 } } },
  { "^\\(a*\\)\\1\\{9\\}\\(a\\{0,9\\}\\)\\([0-9]*;.*[^a]\\2\\([0-9]\\)\\)",
    "a1;;0a1aa2aaa3aaaa4aaaaa5aaaaaa6aaaaaaa7aaaaaaaa8aaaaaaaaa9aa2aa1a0", 0,
    5, { { 0, 67 }, { 0, 0 }, { 0, 1 }, { 1, 67 }, { 66, 67 } } },
   
  { "\\(^\\|foo\\)bar", "bar", 0, 2, { { 0, 3 }, { -1, -1 } } },
  { "\\(foo\\|^\\)bar", "bar", 0, 2, { { 0, 3 }, { -1, -1 } } },
   
  { "(^|foo)bar", "bar", REG_EXTENDED, 2, { { 0, 3 }, { -1, -1 } } },
  { "(foo|^)bar", "bar", REG_EXTENDED, 2, { { 0, 3 }, { -1, -1 } } },
   
  { "(^|foo)bar", "(^|foo)bar", 0, 2, { { 0, 10 }, { -1, -1 } } },
  { "(foo|^)bar", "(foo|^)bar", 0, 2, { { 0, 10 }, { -1, -1 } } },
   
  { "()\\1", "x", REG_EXTENDED, 2, { { 0, 0 }, { 0, 0 } } },
  { "()x\\1", "x", REG_EXTENDED, 2, { { 0, 1 }, { 0, 0 } } },
  { "()\\1*\\1*", "", REG_EXTENDED, 2, { { 0, 0 }, { 0, 0 } } },
  { "([0-9]).*\\1(a*)", "7;7a6", REG_EXTENDED, 3, { { 0, 4 }, { 0, 1 }, { 3, 4 } } },
  { "([0-9]).*\\1(a*)", "7;7a", REG_EXTENDED, 3, { { 0, 4 }, { 0, 1 }, { 3, 4 } } },
  { "(b)()c\\1", "bcb", REG_EXTENDED, 3, { { 0, 3 }, { 0, 1 }, { 1, 1 } } },
  { "()(b)c\\2", "bcb", REG_EXTENDED, 3, { { 0, 3 }, { 0, 0 }, { 0, 1 } } },
  { "a(b)()c\\1", "abcb", REG_EXTENDED, 3, { { 0, 4 }, { 1, 2 }, { 2, 2 } } },
  { "a()(b)c\\2", "abcb", REG_EXTENDED, 3, { { 0, 4 }, { 1, 1 }, { 1, 2 } } },
  { "()(b)\\1c\\2", "bcb", REG_EXTENDED, 3, { { 0, 3 }, { 0, 0 }, { 0, 1 } } },
  { "(b())\\2\\1", "bbbb", REG_EXTENDED, 3, { { 0, 2 }, { 0, 1 }, { 1, 1 } } },
  { "a()(b)\\1c\\2", "abcb", REG_EXTENDED, 3, { { 0, 4 }, { 1, 1 }, { 1, 2 } } },
  { "a()d(b)\\1c\\2", "adbcb", REG_EXTENDED, 3, { { 0, 5 }, { 1, 1 }, { 2, 3 } } },
  { "a(b())\\2\\1", "abbbb", REG_EXTENDED, 3, { { 0, 3 }, { 1, 2 }, { 2, 2 } } },
  { "(bb())\\2\\1", "bbbb", REG_EXTENDED, 3, { { 0, 4 }, { 0, 2 }, { 2, 2 } } },
  { "^([^,]*),\\1,\\1$", "a,a,a", REG_EXTENDED, 2, { { 0, 5 }, { 0, 1 } } },
  { "^([^,]*),\\1,\\1$", "ab,ab,ab", REG_EXTENDED, 2, { { 0, 8 }, { 0, 2 } } },
  { "^([^,]*),\\1,\\1,\\1$", "abc,abc,abc,abc", REG_EXTENDED, 2,
    { { 0, 15 }, { 0, 3 } } },
  { "^(.?)(.?)(.?)(.?)(.?).?\\5\\4\\3\\2\\1$",
    "level", REG_NOSUB | REG_EXTENDED, 0, { { -1, -1 } } },
  { "^(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.).?\\9\\8\\7\\6\\5\\4\\3\\2\\1$|^.?$",
    "level", REG_NOSUB | REG_EXTENDED, 0, { { -1, -1 } } },
  { "^(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.).?\\9\\8\\7\\6\\5\\4\\3\\2\\1$|^.?$",
    "abcdedcba", REG_EXTENDED, 1, { { 0, 9 } } },
  { "^(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.).?\\9\\8\\7\\6\\5\\4\\3\\2\\1$|^.?$",
    "ababababa", REG_EXTENDED, 1, { { 0, 9 } } },
  { "^(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?).?\\9\\8\\7\\6\\5\\4\\3\\2\\1$",
    "level", REG_NOSUB | REG_EXTENDED, 0, { { -1, -1 } } },
  { "^(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?)(.?).?\\9\\8\\7\\6\\5\\4\\3\\2\\1$",
    "ababababa", REG_EXTENDED, 1, { { 0, 9 } } },
   
  { "^a*+(.)", "ab", REG_EXTENDED, 2, { { 0, 2 }, { 1, 2 } } },
   
  { "^(a*)*(.)", "ab", REG_EXTENDED, 3, { { 0, 2 }, { 0, 1 }, { 1, 2 } } },
};

static void
bug_regex11 (void)
{
  regex_t re;
  regmatch_t rm[5];
  size_t i;
  int n;

  for (i = 0; i < sizeof (tests) / sizeof (tests[0]); ++i)
    {
      n = regcomp (&re, tests[i].pattern, tests[i].flags);
      if (n != 0)
        {
          char buf[500];
          regerror (n, &re, buf, sizeof (buf));
          report_error ("%s: regcomp %zd failed: %s", tests[i].pattern, i, buf);
          continue;
        }

      if (regexec (&re, tests[i].string, tests[i].nmatch, rm, 0))
        {
          report_error ("%s: regexec %zd failed", tests[i].pattern, i);
          regfree (&re);
          continue;
        }

      for (n = 0; n < tests[i].nmatch; ++n)
        if (rm[n].rm_so != tests[i].rm[n].rm_so
              || rm[n].rm_eo != tests[i].rm[n].rm_eo)
          {
            if (tests[i].rm[n].rm_so == -1 && tests[i].rm[n].rm_eo == -1)
              break;
            report_error ("%s: regexec %zd match failure rm[%d] %d..%d",
                          tests[i].pattern, i, n,
                          (int) rm[n].rm_so, (int) rm[n].rm_eo);
            break;
          }

      regfree (&re);
    }
}

int
main (void)
{
  static struct re_pattern_buffer regex;
  unsigned char folded_chars[UCHAR_MAX + 1];
  int i;
  const char *s;
  struct re_registers regs;

#if HAVE_DECL_ALARM
   
  int alarm_value = 1000;
  signal (SIGALRM, SIG_DFL);
  alarm (alarm_value);
#endif

  bug_regex11 ();

  if (setlocale (LC_ALL, "en_US.UTF-8"))
    {
      {
         
        static char const pat[] = "insert into";
        static char const data[] =
          "\xFF\0\x12\xA2\xAA\xC4\xB1,K\x12\xC4\xB1*\xACK";
        re_set_syntax (RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE
                       | RE_ICASE);
        memset (&regex, 0, sizeof regex);
        s = re_compile_pattern (pat, sizeof pat - 1, &regex);
        if (s)
          report_error ("%s: %s", pat, s);
        else
          {
            memset (&regs, 0, sizeof regs);
            i = re_search (&regex, data, sizeof data - 1,
                           0, sizeof data - 1, &regs);
            if (i != -1)
              report_error ("re_search '%s' on '%s' returned %d",
                            pat, data, i);
            regfree (&regex);
            free (regs.start);
            free (regs.end);
          }
      }

      if (really_utf8 ())
        {
           
          static char const pat[] = "[^x]x";
          static char const data[] =
             
            "\xe1\x80\x80"
            "\xe1\x80\xbb"
            "\xe1\x80\xbd"
            "\xe1\x80\x94"
            "\xe1\x80\xba"
            "\xe1\x80\xaf"
            "\xe1\x80\x95"
            "\xe1\x80\xba"
            "x";
          re_set_syntax (0);
          memset (&regex, 0, sizeof regex);
          s = re_compile_pattern (pat, sizeof pat - 1, &regex);
          if (s)
            report_error ("%s: %s", pat, s);
          else
            {
              memset (&regs, 0, sizeof regs);
              i = re_search (&regex, data, sizeof data - 1,
                             0, sizeof data - 1, 0);
              if (i != 0 && i != 21)
                report_error ("re_search '%s' on '%s' returned %d",
                              pat, data, i);
              regfree (&regex);
              free (regs.start);
              free (regs.end);
            }
        }

      if (! setlocale (LC_ALL, "C"))
        {
          report_error ("setlocale \"C\" failed");
          return exit_status;
        }
    }

  if (setlocale (LC_ALL, "tr_TR.UTF-8"))
    {
      if (really_utf8 () && towupper (L'i') == 0x0130  )
        {
          re_set_syntax (RE_SYNTAX_GREP | RE_ICASE);
          memset (&regex, 0, sizeof regex);
          static char const pat[] = "i";
          s = re_compile_pattern (pat, sizeof pat - 1, &regex);
          if (s)
            report_error ("%s: %s", pat, s);
          else
            {
               
  re_set_syntax (RE_SYNTAX_EGREP | RE_HAT_LISTS_NOT_NEWLINE);
  memset (&regex, 0, sizeof regex);
  static char const pat_3957[] = "a[^x]b";
  s = re_compile_pattern (pat_3957, sizeof pat_3957 - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_3957, s);
  else
    {
       
      memset (&regs, 0, sizeof regs);
      static char const data[] = "a\nb";
      i = re_search (&regex, data, sizeof data - 1, 0, sizeof data - 1, &regs);
      if (i != -1)
        report_error ("re_search '%s' on '%s' returned %d",
                      pat_3957, data, i);
      regfree (&regex);
      free (regs.start);
      free (regs.end);
    }

   
  re_set_syntax (RE_SYNTAX_POSIX_EGREP);
  memset (&regex, 0, sizeof regex);
  for (i = 0; i <= UCHAR_MAX; i++)
    folded_chars[i] = i;
  regex.translate = folded_chars;
  static char const pat75[] = "a[[:@:>@:]]b\n";
  s = re_compile_pattern (pat75, sizeof pat75 - 1, &regex);
   
  if (!s)
    {
      report_error ("re_compile_pattern: failed to reject '%s'", pat75);
      regfree (&regex);
    }

   
  re_set_syntax (RE_SYNTAX_POSIX_EGREP | RE_NO_EMPTY_RANGES);
  memset (&regex, 0, sizeof regex);
  static char const pat_b_a[] = "a[b-a]";
  s = re_compile_pattern (pat_b_a, sizeof pat_b_a - 1, &regex);
  if (s == 0)
    {
      report_error ("re_compile_pattern: failed to reject '%s'", pat_b_a);
      regfree (&regex);
    }

   
  memset (&regex, 0, sizeof regex);
  static char const pat_213[] = "{1";
  s = re_compile_pattern (pat_213, sizeof pat_213 - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_213, s);
  else
    regfree (&regex);

   
  memset (&regex, 0, sizeof regex);
  static char const pat_stolfi[] = "[an\371]*n";
  s = re_compile_pattern (pat_stolfi, sizeof pat_stolfi - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_stolfi, s);
   
  else
    {
      memset (&regs, 0, sizeof regs);
      static char const data[] = "an";
      i = re_match (&regex, data, sizeof data - 1, 0, &regs);
      if (i != 2)
        report_error ("re_match '%s' on '%s' at 2 returned %d",
                      pat_stolfi, data, i);
      regfree (&regex);
      free (regs.start);
      free (regs.end);
    }

  memset (&regex, 0, sizeof regex);
  static char const pat_x[] = "x";
  s = re_compile_pattern (pat_x, sizeof pat_x - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_x, s);
   
  else
    {
      memset (&regs, 0, sizeof regs);
      static char const data[] = "wxy";
      i = re_search (&regex, data, sizeof data - 1, 2, -2, &regs);
      if (i != 1)
        report_error ("re_search '%s' on '%s' returned %d", pat_x, data, i);
      regfree (&regex);
      free (regs.start);
      free (regs.end);
    }

   
  re_set_syntax (RE_SYNTAX_EMACS | RE_ICASE);
  memset (&regex, 0, sizeof regex);
  s = re_compile_pattern (pat_x, 1, &regex);
  if (s)
    report_error ("%s: %s", pat_x, s);
  else
    {
      memset (&regs, 0, sizeof regs);
      static char const data[] = "WXY";
      i = re_search (&regex, data, sizeof data - 1, 0, 3, &regs);
      if (i < 0)
        report_error ("re_search '%s' on '%s' returned %d", pat_x, data, i);
      regfree (&regex);
      free (regs.start);
      free (regs.end);
    }

   
  re_set_syntax (RE_SYNTAX_POSIX_BASIC);
  memset (&regex, 0, sizeof regex);
  static char const pat_sub2[] = "\\(a*\\)*a*\\1";
  s = re_compile_pattern (pat_sub2, sizeof pat_sub2 - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_sub2, s);
  else
    {
      memset (&regs, 0, sizeof regs);
      static char const data[] = "a";
      int datalen = sizeof data - 1;
      i = re_search (&regex, data, datalen, 0, datalen, &regs);
      if (i != 0)
        report_error ("re_search '%s' on '%s' returned %d", pat_sub2, data, i);
      else if (regs.num_regs < 2)
        report_error ("re_search '%s' on '%s' returned only %d registers",
                      pat_sub2, data, (int) regs.num_regs);
      else if (! (regs.start[0] == 0 && regs.end[0] == 1))
        report_error ("re_search '%s' on '%s' returned wrong match [%d,%d)",
                      pat_sub2, data, (int) regs.start[0], (int) regs.end[0]);
      else if (! (regs.start[1] == 0 && regs.end[1] == 0))
        report_error ("re_search '%s' on '%s' returned wrong submatch [%d,%d)",
                      pat_sub2, data, (int) regs.start[1], (int) regs.end[1]);
      regfree (&regex);
      free (regs.start);
      free (regs.end);
    }

   
  re_set_syntax (RE_SYNTAX_POSIX_BASIC
                 & ~RE_CONTEXT_INVALID_DUP
                 & ~RE_NO_EMPTY_RANGES);
  static char const pat_shelton[] = "[[:alnum:]_-]\\\\+$";
  s = re_compile_pattern (pat_shelton, sizeof pat_shelton - 1, &regex);
  if (s)
    report_error ("%s: %s", pat_shelton, s);
  else
    regfree (&regex);

   
  if (REG_STARTEND == 0)
    report_error ("REG_STARTEND is zero");

   
  re_set_syntax (RE_SYNTAX_POSIX_EGREP);
  memset (&regex, 0, sizeof regex);
  static char const pat_badback[] = "0|()0|\\1|0";
  s = re_compile_pattern (pat_badback, sizeof pat_badback, &regex);
  if (!s && re_search (&regex, "x", 1, 0, 1, &regs) != -1)
    s = "mishandled invalid back reference";
  if (s && strcmp (s, "Invalid back reference") != 0)
    report_error ("%s: %s", pat_badback, s);

#if 0
   
  if (sizeof (regoff_t) < sizeof (ptrdiff_t)
      || sizeof (regoff_t) < sizeof (ssize_t))
    report_error ("regoff_t values are too narrow");
#endif

  return exit_status;
}
