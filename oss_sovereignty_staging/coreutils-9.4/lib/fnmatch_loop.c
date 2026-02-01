 
static int FCT (const CHAR *pattern, const CHAR *string,
                const CHAR *string_end, bool no_leading_period, int flags,
                struct STRUCT *ends, size_t alloca_used);
static int EXT (INT opt, const CHAR *pattern, const CHAR *string,
                const CHAR *string_end, bool no_leading_period, int flags,
                size_t alloca_used);
static const CHAR *END (const CHAR *patternp);

static int
FCT (const CHAR *pattern, const CHAR *string, const CHAR *string_end,
     bool no_leading_period, int flags, struct STRUCT *ends, size_t alloca_used)
{
  const CHAR *p = pattern, *n = string;
  UCHAR c;
#ifdef _LIBC
# if WIDE_CHAR_VERSION
  const char *collseq = (const char *)
    _NL_CURRENT(LC_COLLATE, _NL_COLLATE_COLLSEQWC);
# else
  const UCHAR *collseq = (const UCHAR *)
    _NL_CURRENT(LC_COLLATE, _NL_COLLATE_COLLSEQMB);
# endif
#endif

  while ((c = *p++) != L_('\0'))
    {
      bool new_no_leading_period = false;
      c = FOLD (c);

      switch (c)
        {
        case L_('?'):
          if (__glibc_unlikely (flags & FNM_EXTMATCH) && *p == '(')
            {
              int res = EXT (c, p, n, string_end, no_leading_period,
                             flags, alloca_used);
              if (res != -1)
                return res;
            }

          if (n == string_end)
            return FNM_NOMATCH;
          else if (*n == L_('/') && (flags & FNM_FILE_NAME))
            return FNM_NOMATCH;
          else if (*n == L_('.') && no_leading_period)
            return FNM_NOMATCH;
          break;

        case L_('\\'):
          if (!(flags & FNM_NOESCAPE))
            {
              c = *p++;
              if (c == L_('\0'))
                 
                return FNM_NOMATCH;
              c = FOLD (c);
            }
          if (n == string_end || FOLD ((UCHAR) *n) != c)
            return FNM_NOMATCH;
          break;

        case L_('*'):
          if (__glibc_unlikely (flags & FNM_EXTMATCH) && *p == '(')
            {
              int res = EXT (c, p, n, string_end, no_leading_period,
                             flags, alloca_used);
              if (res != -1)
                return res;
            }
          else if (ends != NULL)
            {
              ends->pattern = p - 1;
              ends->string = n;
              ends->no_leading_period = no_leading_period;
              return 0;
            }

          if (n != string_end && *n == L_('.') && no_leading_period)
            return FNM_NOMATCH;

          for (c = *p++; c == L_('?') || c == L_('*'); c = *p++)
            {
              if (*p == L_('(') && (flags & FNM_EXTMATCH) != 0)
                {
                  const CHAR *endp = END (p);
                  if (endp != p)
                    {
                       
                      p = endp;
                      continue;
                    }
                }

              if (c == L_('?'))
                {
                   
                  if (n == string_end)
                     
                    return FNM_NOMATCH;
                  else if (*n == L_('/')
                           && __glibc_unlikely (flags & FNM_FILE_NAME))
                     
                    return FNM_NOMATCH;
                  else
                     
                    ++n;
                }
            }

          if (c == L_('\0'))
             
            {
              int result = (flags & FNM_FILE_NAME) == 0 ? 0 : FNM_NOMATCH;

              if (flags & FNM_FILE_NAME)
                {
                  if (flags & FNM_LEADING_DIR)
                    result = 0;
                  else
                    {
                      if (MEMCHR (n, L_('/'), string_end - n) == NULL)
                        result = 0;
                    }
                }

              return result;
            }
          else
            {
              const CHAR *endp;
              struct STRUCT end;

              end.pattern = NULL;
              endp = MEMCHR (n, (flags & FNM_FILE_NAME) ? L_('/') : L_('\0'),
                             string_end - n);
              if (endp == NULL)
                endp = string_end;

              if (c == L_('[')
                  || (__glibc_unlikely (flags & FNM_EXTMATCH)
                      && (c == L_('@') || c == L_('+') || c == L_('!'))
                      && *p == L_('(')))
                {
                  int flags2 = ((flags & FNM_FILE_NAME)
                                ? flags : (flags & ~FNM_PERIOD));

                  for (--p; n < endp; ++n, no_leading_period = false)
                    if (FCT (p, n, string_end, no_leading_period, flags2,
                             &end, alloca_used) == 0)
                      goto found;
                }
              else if (c == L_('/') && (flags & FNM_FILE_NAME))
                {
                  while (n < string_end && *n != L_('/'))
                    ++n;
                  if (n < string_end && *n == L_('/')
                      && (FCT (p, n + 1, string_end, flags & FNM_PERIOD, flags,
                               NULL, alloca_used) == 0))
                    return 0;
                }
              else
                {
                  int flags2 = ((flags & FNM_FILE_NAME)
                                ? flags : (flags & ~FNM_PERIOD));

                  if (c == L_('\\') && !(flags & FNM_NOESCAPE))
                    c = *p;
                  c = FOLD (c);
                  for (--p; n < endp; ++n, no_leading_period = false)
                    if (FOLD ((UCHAR) *n) == c
                        && (FCT (p, n, string_end, no_leading_period, flags2,
                                 &end, alloca_used) == 0))
                      {
                      found:
                        if (end.pattern == NULL)
                          return 0;
                        break;
                      }
                  if (end.pattern != NULL)
                    {
                      p = end.pattern;
                      n = end.string;
                      no_leading_period = end.no_leading_period;
                      continue;
                    }
                }
            }

           
          return FNM_NOMATCH;

        case L_('['):
          {
             
            const CHAR *p_init = p;
            const CHAR *n_init = n;
            bool not;
            CHAR cold;
            UCHAR fn;

            if (posixly_correct == 0)
              posixly_correct = getenv ("POSIXLY_CORRECT") != NULL ? 1 : -1;

            if (n == string_end)
              return FNM_NOMATCH;

            if (*n == L_('.') && no_leading_period)
              return FNM_NOMATCH;

            if (*n == L_('/') && (flags & FNM_FILE_NAME))
               
              return FNM_NOMATCH;

            not = (*p == L_('!') || (posixly_correct < 0 && *p == L_('^')));
            if (not)
              ++p;

            fn = FOLD ((UCHAR) *n);

            c = *p++;
            for (;;)
              {
                if (!(flags & FNM_NOESCAPE) && c == L_('\\'))
                  {
                    if (*p == L_('\0'))
                      return FNM_NOMATCH;
                    c = FOLD ((UCHAR) *p);
                    ++p;

                    goto normal_bracket;
                  }
                else if (c == L_('[') && *p == L_(':'))
                  {
                     
                    CHAR str[CHAR_CLASS_MAX_LENGTH + 1];
                    size_t c1 = 0;
                    WCTYPE_T wt;
                    const CHAR *startp = p;

                    for (;;)
                      {
                        if (c1 == CHAR_CLASS_MAX_LENGTH)
                           
                          return FNM_NOMATCH;

                        c = *++p;
                        if (c == L_(':') && p[1] == L_(']'))
                          {
                            p += 2;
                            break;
                          }
                        if (c < L_('a') || c >= L_('z'))
                          {
                             
                            p = startp;
                            c = L_('[');
                            goto normal_bracket;
                          }
                        str[c1++] = c;
                      }
                    str[c1] = L_('\0');

                    wt = IS_CHAR_CLASS (str);
                    if (wt == 0)
                       
                      return FNM_NOMATCH;

#if defined _LIBC && ! WIDE_CHAR_VERSION
                     
                    if (_ISCTYPE ((UCHAR) *n, wt))
                      goto matched;
#else
                    if (ISWCTYPE (UCHAR_TO_WCHAR ((UCHAR) *n), wt))
                      goto matched;
#endif
                    c = *p++;
                  }
#ifdef _LIBC
                else if (c == L_('[') && *p == L_('='))
                  {
                     
                    UCHAR str;
                    uint32_t nrules =
                      _NL_CURRENT_WORD (LC_COLLATE, _NL_COLLATE_NRULES);
                    const CHAR *startp = p;

                    c = *++p;
                    if (c == L_('\0'))
                      {
                        p = startp;
                        c = L_('[');
                        goto normal_bracket;
                      }
                    str = c;

                    c = *++p;
                    if (c != L_('=') || p[1] != L_(']'))
                      {
                        p = startp;
                        c = L_('[');
                        goto normal_bracket;
                      }
                    p += 2;

                    if (nrules == 0)
                      {
                        if ((UCHAR) *n == str)
                          goto matched;
                      }
                    else
                      {
                        const int32_t *table;
# if WIDE_CHAR_VERSION
                        const int32_t *weights;
                        const wint_t *extra;
# else
                        const unsigned char *weights;
                        const unsigned char *extra;
# endif
                        const int32_t *indirect;
                        int32_t idx;
                        const UCHAR *cp = (const UCHAR *) &str;

# if WIDE_CHAR_VERSION
                        table = (const int32_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_TABLEWC);
                        weights = (const int32_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_WEIGHTWC);
                        extra = (const wint_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_EXTRAWC);
                        indirect = (const int32_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_INDIRECTWC);
# else
                        table = (const int32_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_TABLEMB);
                        weights = (const unsigned char *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_WEIGHTMB);
                        extra = (const unsigned char *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_EXTRAMB);
                        indirect = (const int32_t *)
                          _NL_CURRENT (LC_COLLATE, _NL_COLLATE_INDIRECTMB);
# endif

                        idx = FINDIDX (table, indirect, extra, &cp, 1);
                        if (idx != 0)
                          {
                             
                            int len = weights[idx & 0xffffff];
                            int32_t idx2;
                            const UCHAR *np = (const UCHAR *) n;

                            idx2 = FINDIDX (table, indirect, extra,
                                            &np, string_end - n);
                            if (idx2 != 0
                                && (idx >> 24) == (idx2 >> 24)
                                && len == weights[idx2 & 0xffffff])
                              {
                                int cnt = 0;

                                idx &= 0xffffff;
                                idx2 &= 0xffffff;

                                while (cnt < len
                                       && (weights[idx + 1 + cnt]
                                           == weights[idx2 + 1 + cnt]))
                                  ++cnt;

                                if (cnt == len)
                                  goto matched;
                              }
                          }
                      }

                    c = *p++;
                  }
#endif
                else if (c == L_('\0'))
                  {
                     
                    p = p_init;
                    n = n_init;
                    c = L_('[');
                    goto normal_match;
                  }
                else
                  {
                    bool is_range = false;

#ifdef _LIBC
                    bool is_seqval = false;

                    if (c == L_('[') && *p == L_('.'))
                      {
                        uint32_t nrules =
                          _NL_CURRENT_WORD (LC_COLLATE, _NL_COLLATE_NRULES);
                        const CHAR *startp = p;
                        size_t c1 = 0;

                        while (1)
                          {
                            c = *++p;
                            if (c == L_('.') && p[1] == L_(']'))
                              {
                                p += 2;
                                break;
                              }
                            if (c == '\0')
                              return FNM_NOMATCH;
                            ++c1;
                          }

                         
                        is_range = *p == L_('-') && p[1] != L_('\0');

                        if (nrules == 0)
                          {
                             
                            if (c1 != 1)
                              return FNM_NOMATCH;

                            if (!is_range && *n == startp[1])
                              goto matched;

                            cold = startp[1];
                            c = *p++;
                          }
                        else
                          {
                            int32_t table_size;
                            const int32_t *symb_table;
                            const unsigned char *extra;
                            int32_t idx;
                            int32_t elem;
# if WIDE_CHAR_VERSION
                            CHAR *wextra;
# endif

                            table_size =
                              _NL_CURRENT_WORD (LC_COLLATE,
                                                _NL_COLLATE_SYMB_HASH_SIZEMB);
                            symb_table = (const int32_t *)
                              _NL_CURRENT (LC_COLLATE,
                                           _NL_COLLATE_SYMB_TABLEMB);
                            extra = (const unsigned char *)
                              _NL_CURRENT (LC_COLLATE,
                                           _NL_COLLATE_SYMB_EXTRAMB);

                            for (elem = 0; elem < table_size; elem++)
                              if (symb_table[2 * elem] != 0)
                                {
                                  idx = symb_table[2 * elem + 1];
                                   
                                  idx += 1 + extra[idx];
# if WIDE_CHAR_VERSION
                                   
                                  idx += 1 + extra[idx];
                                   
                                  idx = (idx + 3) & ~3;

                                  wextra = (CHAR *) &extra[idx + 4];

                                  if ( 
                                      c1 == wextra[0]
                                       
                                      && (__wmemcmp (startp + 1, &wextra[1],
                                                     c1)
                                          == 0))
                                     
                                    break;
# else
                                  if ( 
                                      c1 == extra[idx]
                                       
                                      && memcmp (startp + 1,
                                                 &extra[idx + 1], c1) == 0)
                                     
                                    break;
# endif
                                }

                            if (elem < table_size)
                              {
                                 
                                if (! is_range

# if WIDE_CHAR_VERSION
                                    && __wmemcmp (n, &wextra[1], c1) == 0
# else
                                    && memcmp (n, &extra[idx + 1], c1) == 0
# endif
                                    )
                                  {
                                    n += c1 - 1;
                                    goto matched;
                                  }

                                 
                                is_seqval = true;
# if WIDE_CHAR_VERSION
                                cold = wextra[1 + wextra[0]];
# else
                                idx += 1 + extra[idx];
                                 
                                idx = (idx + 3) & ~3;
                                cold = *((int32_t *) &extra[idx]);
# endif

                                c = *p++;
                              }
                            else if (c1 == 1)
                              {
                                 
                                if (!is_range && *n == startp[1])
                                  goto matched;

                                cold = startp[1];
                                c = *p++;
                              }
                            else
                              return FNM_NOMATCH;
                          }
                      }
                    else
#endif
                      {
                        c = FOLD (c);
                      normal_bracket:

                         
                        is_range = (*p == L_('-') && p[1] != L_('\0')
                                    && p[1] != L_(']'));

                        if (!is_range && c == fn)
                          goto matched;

#if _LIBC
                         
                        is_seqval = false;
#endif
                        cold = c;
                        c = *p++;
                      }

                    if (c == L_('-') && *p != L_(']'))
                      {
#if _LIBC
                         
                        uint32_t fcollseq;
                        uint32_t lcollseq;
                        UCHAR cend = *p++;

# if WIDE_CHAR_VERSION
                         
                        fcollseq = __collseq_table_lookup (collseq, fn);
                        if (fcollseq == ~((uint32_t) 0))
                           
                          goto range_not_matched;

                        if (is_seqval)
                          lcollseq = cold;
                        else
                          lcollseq = __collseq_table_lookup (collseq, cold);
# else
                        fcollseq = collseq[fn];
                        lcollseq = is_seqval ? cold : collseq[(UCHAR) cold];
# endif

                        is_seqval = false;
                        if (cend == L_('[') && *p == L_('.'))
                          {
                            uint32_t nrules =
                              _NL_CURRENT_WORD (LC_COLLATE,
                                                _NL_COLLATE_NRULES);
                            const CHAR *startp = p;
                            size_t c1 = 0;

                            while (1)
                              {
                                c = *++p;
                                if (c == L_('.') && p[1] == L_(']'))
                                  {
                                    p += 2;
                                    break;
                                  }
                                if (c == '\0')
                                  return FNM_NOMATCH;
                                ++c1;
                              }

                            if (nrules == 0)
                              {
                                 
                                if (c1 != 1)
                                  return FNM_NOMATCH;

                                cend = startp[1];
                              }
                            else
                              {
                                int32_t table_size;
                                const int32_t *symb_table;
                                const unsigned char *extra;
                                int32_t idx;
                                int32_t elem;
# if WIDE_CHAR_VERSION
                                CHAR *wextra;
# endif

                                table_size =
                                  _NL_CURRENT_WORD (LC_COLLATE,
                                                    _NL_COLLATE_SYMB_HASH_SIZEMB);
                                symb_table = (const int32_t *)
                                  _NL_CURRENT (LC_COLLATE,
                                               _NL_COLLATE_SYMB_TABLEMB);
                                extra = (const unsigned char *)
                                  _NL_CURRENT (LC_COLLATE,
                                               _NL_COLLATE_SYMB_EXTRAMB);

                                for (elem = 0; elem < table_size; elem++)
                                  if (symb_table[2 * elem] != 0)
                                    {
                                      idx = symb_table[2 * elem + 1];
                                       
                                      idx += 1 + extra[idx];
# if WIDE_CHAR_VERSION
                                       
                                      idx += 1 + extra[idx];
                                       
                                      idx = (idx + 3) & ~3;

                                      wextra = (CHAR *) &extra[idx + 4];

                                      if ( 
                                          c1 == wextra[0]
                                           
                                          && (__wmemcmp (startp + 1,
                                                         &wextra[1], c1)
                                              == 0))
                                         
                                        break;
# else
                                      if ( 
                                          c1 == extra[idx]
                                           
                                          && memcmp (startp + 1,
                                                     &extra[idx + 1], c1) == 0)
                                         
                                        break;
# endif
                                    }

                                if (elem < table_size)
                                  {
                                     
                                    is_seqval = true;
# if WIDE_CHAR_VERSION
                                    cend = wextra[1 + wextra[0]];
# else
                                    idx += 1 + extra[idx];
                                     
                                    idx = (idx + 3) & ~3;
                                    cend = *((int32_t *) &extra[idx]);
# endif
                                  }
                                else if (c1 == 1)
                                  {
                                    cend = startp[1];
                                    c = *p++;
                                  }
                                else
                                  return FNM_NOMATCH;
                              }
                          }
                        else
                          {
                            if (!(flags & FNM_NOESCAPE) && cend == L_('\\'))
                              cend = *p++;
                            if (cend == L_('\0'))
                              return FNM_NOMATCH;
                            cend = FOLD (cend);
                          }

                         
                        if (
# if WIDE_CHAR_VERSION
                            lcollseq == 0xffffffff ||
# endif
                            lcollseq <= fcollseq)
                          {
                             
                            uint32_t hcollseq;

                            if (is_seqval)
                              hcollseq = cend;
                            else
                              {
# if WIDE_CHAR_VERSION
                                hcollseq =
                                  __collseq_table_lookup (collseq, cend);
                                if (hcollseq == ~((uint32_t) 0))
                                  {
                                     
                                    if (lcollseq != fcollseq)
                                      goto range_not_matched;

                                    goto matched;
                                  }
# else
                                hcollseq = collseq[cend];
# endif
                              }

                            if (lcollseq <= hcollseq && fcollseq <= hcollseq)
                              goto matched;
                          }
# if WIDE_CHAR_VERSION
                      range_not_matched:
# endif
#else
                         
                        UCHAR cend = *p++;

                        if (!(flags & FNM_NOESCAPE) && cend == L_('\\'))
                          cend = *p++;
                        if (cend == L_('\0'))
                          return FNM_NOMATCH;

                         
                        if ((UCHAR) cold <= fn && fn <= cend)
                          goto matched;
#endif

                        c = *p++;
                      }
                  }

                if (c == L_(']'))
                  break;
              }

            if (!not)
              return FNM_NOMATCH;
            break;

          matched:
             
            while ((c = *p++) != L_(']'))
              {
                if (c == L_('\0'))
                   
                  return FNM_NOMATCH;

                if (!(flags & FNM_NOESCAPE) && c == L_('\\'))
                  {
                    if (*p == L_('\0'))
                      return FNM_NOMATCH;
                     
                    ++p;
                  }
                else if (c == L_('[') && *p == L_(':'))
                  {
                    int c1 = 0;
                    const CHAR *startp = p;

                    while (1)
                      {
                        c = *++p;
                        if (++c1 == CHAR_CLASS_MAX_LENGTH)
                          return FNM_NOMATCH;

                        if (*p == L_(':') && p[1] == L_(']'))
                          break;

                        if (c < L_('a') || c >= L_('z'))
                          {
                            p = startp - 2;
                            break;
                          }
                      }
                    p += 2;
                  }
                else if (c == L_('[') && *p == L_('='))
                  {
                    c = *++p;
                    if (c == L_('\0'))
                      return FNM_NOMATCH;
                    c = *++p;
                    if (c != L_('=') || p[1] != L_(']'))
                      return FNM_NOMATCH;
                    p += 2;
                  }
                else if (c == L_('[') && *p == L_('.'))
                  {
                    while (1)
                      {
                        c = *++p;
                        if (c == L_('\0'))
                          return FNM_NOMATCH;

                        if (c == L_('.') && p[1] == L_(']'))
                          break;
                      }
                    p += 2;
                  }
              }
            if (not)
              return FNM_NOMATCH;
          }
          break;

        case L_('+'):
        case L_('@'):
        case L_('!'):
          if (__glibc_unlikely (flags & FNM_EXTMATCH) && *p == '(')
            {
              int res = EXT (c, p, n, string_end, no_leading_period, flags,
                             alloca_used);
              if (res != -1)
                return res;
            }
          goto normal_match;

        case L_('/'):
          if (NO_LEADING_PERIOD (flags))
            {
              if (n == string_end || c != (UCHAR) *n)
                return FNM_NOMATCH;

              new_no_leading_period = true;
              break;
            }
          FALLTHROUGH;
        default:
        normal_match:
          if (n == string_end || c != FOLD ((UCHAR) *n))
            return FNM_NOMATCH;
        }

      no_leading_period = new_no_leading_period;
      ++n;
    }

  if (n == string_end)
    return 0;

  if ((flags & FNM_LEADING_DIR) && n != string_end && *n == L_('/'))
     
    return 0;

  return FNM_NOMATCH;
}


static const CHAR *
END (const CHAR *pattern)
{
  const CHAR *p = pattern;

  while (1)
    if (*++p == L_('\0'))
       
      return pattern;
    else if (*p == L_('['))
      {
         
        if (posixly_correct == 0)
          posixly_correct = getenv ("POSIXLY_CORRECT") != NULL ? 1 : -1;

         
        if (*++p == L_('!') || (posixly_correct < 0 && *p == L_('^')))
          ++p;
         
        if (*p == L_(']'))
          ++p;
         
        while (*p != L_(']'))
          if (*p++ == L_('\0'))
             
            return pattern;
      }
    else if ((*p == L_('?') || *p == L_('*') || *p == L_('+') || *p == L_('@')
              || *p == L_('!')) && p[1] == L_('('))
      {
        p = END (p + 1);
        if (*p == L_('\0'))
           
          return pattern;
      }
    else if (*p == L_(')'))
      break;

  return p + 1;
}


static int
EXT (INT opt, const CHAR *pattern, const CHAR *string, const CHAR *string_end,
     bool no_leading_period, int flags, size_t alloca_used)
{
  const CHAR *startp;
  ptrdiff_t level;
  struct patternlist
  {
    struct patternlist *next;
    CHAR malloced;
    CHAR str __flexarr;
  } *list = NULL;
  struct patternlist **lastp = &list;
  size_t pattern_len = STRLEN (pattern);
  bool any_malloced = false;
  const CHAR *p;
  const CHAR *rs;
  int retval = 0;

   
  level = 0;
  for (startp = p = pattern + 1; level >= 0; ++p)
    if (*p == L_('\0'))
      {
         
        retval = -1;
        goto out;
      }
    else if (*p == L_('['))
      {
         
        if (posixly_correct == 0)
          posixly_correct = getenv ("POSIXLY_CORRECT") != NULL ? 1 : -1;

         
        if (*++p == L_('!') || (posixly_correct < 0 && *p == L_('^')))
          ++p;
         
        if (*p == L_(']'))
          ++p;
         
        while (*p != L_(']'))
          if (*p++ == L_('\0'))
            {
               
              retval = -1;
              goto out;
            }
      }
    else if ((*p == L_('?') || *p == L_('*') || *p == L_('+') || *p == L_('@')
              || *p == L_('!')) && p[1] == L_('('))
       
      ++level;
    else if (*p == L_(')'))
      {
        if (level-- == 0)
          {
             
#define NEW_PATTERN \
            struct patternlist *newp;                                         \
            size_t plen = (opt == L_('?') || opt == L_('@')                   \
                           ? pattern_len : (p - startp + 1UL));               \
            idx_t slen = FLEXSIZEOF (struct patternlist, str, 0);             \
            idx_t new_used = alloca_used + slen;                              \
            idx_t plensize;                                                   \
            if (ckd_mul (&plensize, plen, sizeof (CHAR))                      \
                || ckd_add (&new_used, new_used, plensize))                   \
              {                                                               \
                retval = -2;                                                  \
                goto out;                                                     \
              }                                                               \
            slen += plensize;                                                 \
            bool malloced = ! __libc_use_alloca (new_used);                   \
            if (__glibc_unlikely (malloced))                                  \
              {                                                               \
                newp = malloc (slen);                                         \
                if (newp == NULL)                                             \
                  {                                                           \
                    retval = -2;                                              \
                    goto out;                                                 \
                  }                                                           \
                any_malloced = true;                                          \
              }                                                               \
            else                                                              \
              newp = alloca_account (slen, alloca_used);                      \
            newp->next = NULL;                                                \
            newp->malloced = malloced;                                        \
            *((CHAR *) MEMPCPY (newp->str, startp, p - startp)) = L_('\0');   \
            *lastp = newp;                                                    \
            lastp = &newp->next
            NEW_PATTERN;
          }
      }
    else if (*p == L_('|'))
      {
        if (level == 0)
          {
            NEW_PATTERN;
            startp = p + 1;
          }
      }
  assert (list != NULL);
  assert (p[-1] == L_(')'));
#undef NEW_PATTERN

  switch (opt)
    {
    case L_('*'):
      if (FCT (p, string, string_end, no_leading_period, flags, NULL,
               alloca_used) == 0)
        goto success;
      FALLTHROUGH;
    case L_('+'):
      do
        {
          for (rs = string; rs <= string_end; ++rs)
             
            if (FCT (list->str, string, rs, no_leading_period,
                     flags & FNM_FILE_NAME ? flags : flags & ~FNM_PERIOD,
                     NULL, alloca_used) == 0
                 
                && (FCT (p, rs, string_end,
                         rs == string
                         ? no_leading_period
                         : rs[-1] == '/' && NO_LEADING_PERIOD (flags),
                         flags & FNM_FILE_NAME
                         ? flags : flags & ~FNM_PERIOD, NULL, alloca_used) == 0
                     
                    || (rs != string
                        && FCT (pattern - 1, rs, string_end,
                                rs == string
                                ? no_leading_period
                                : rs[-1] == '/' && NO_LEADING_PERIOD (flags),
                                flags & FNM_FILE_NAME
                                ? flags : flags & ~FNM_PERIOD, NULL,
                                alloca_used) == 0)))
               
              goto success;
        }
      while ((list = list->next) != NULL);

       
      retval = FNM_NOMATCH;
      break;

    case L_('?'):
      if (FCT (p, string, string_end, no_leading_period, flags, NULL,
               alloca_used) == 0)
        goto success;
      FALLTHROUGH;
    case L_('@'):
      do
         
        if (FCT (STRCAT (list->str, p), string, string_end,
                 no_leading_period,
                 flags & FNM_FILE_NAME ? flags : flags & ~FNM_PERIOD,
                 NULL, alloca_used) == 0)
           
          goto success;
      while ((list = list->next) != NULL);

       
      retval = FNM_NOMATCH;
      break;

    case L_('!'):
      for (rs = string; rs <= string_end; ++rs)
        {
          struct patternlist *runp;

          for (runp = list; runp != NULL; runp = runp->next)
            if (FCT (runp->str, string, rs,  no_leading_period,
                     flags & FNM_FILE_NAME ? flags : flags & ~FNM_PERIOD,
                     NULL, alloca_used) == 0)
              break;

           
          if (runp == NULL
              && (FCT (p, rs, string_end,
                       rs == string
                       ? no_leading_period
                       : rs[-1] == '/' && NO_LEADING_PERIOD (flags),
                       flags & FNM_FILE_NAME ? flags : flags & ~FNM_PERIOD,
                       NULL, alloca_used) == 0))
             
            goto success;
        }

       
      retval = FNM_NOMATCH;
      break;

    default:
      assert (! "Invalid extended matching operator");
      retval = -1;
      break;
    }

 success:
 out:
  if (any_malloced)
    while (list != NULL)
      {
        struct patternlist *old = list;
        list = list->next;
        if (old->malloced)
          free (old);
      }

  return retval;
}


#undef FOLD
#undef CHAR
#undef UCHAR
#undef INT
#undef FCT
#undef EXT
#undef END
#undef STRUCT
#undef MEMPCPY
#undef MEMCHR
#undef STRLEN
#undef STRCAT
#undef L_
#undef UCHAR_TO_WCHAR
#undef WIDE_CHAR_VERSION
#undef FINDIDX
