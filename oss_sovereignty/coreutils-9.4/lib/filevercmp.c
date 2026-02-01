 
static idx_t
file_prefixlen (char const *s, ptrdiff_t *len)
{
  size_t n = *len;   
  idx_t prefixlen = 0;

  for (idx_t i = 0; ; )
    {
      if (*len < 0 ? !s[i] : i == n)
        {
          *len = i;
          return prefixlen;
        }

      i++;
      prefixlen = i;
      while (i + 1 < n && s[i] == '.' && (c_isalpha (s[i + 1])
                                          || s[i + 1] == '~'))
        for (i += 2; i < n && (c_isalnum (s[i]) || s[i] == '~'); i++)
          continue;
    }
}

 

static int
order (char const *s, idx_t pos, idx_t len)
{
  if (pos == len)
    return -1;

  unsigned char c = s[pos];
  if (c_isdigit (c))
    return 0;
  else if (c_isalpha (c))
    return c;
  else if (c == '~')
    return -2;
  else
    {
      static_assert (UCHAR_MAX <= (INT_MAX - 1 - 2) / 2);
      return c + UCHAR_MAX + 1;
    }
}

 
int
filevercmp (const char *s1, const char *s2)
{
  return filenvercmp (s1, -1, s2, -1);
}

 
int
filenvercmp (char const *a, ptrdiff_t alen, char const *b, ptrdiff_t blen)
{
   
  bool aempty = alen < 0 ? !a[0] : !alen;
  bool bempty = blen < 0 ? !b[0] : !blen;
  if (aempty)
    return -!bempty;
  if (bempty)
    return 1;

   
  if (a[0] == '.')
    {
      if (b[0] != '.')
        return -1;

      bool adot = alen < 0 ? !a[1] : alen == 1;
      bool bdot = blen < 0 ? !b[1] : blen == 1;
      if (adot)
        return -!bdot;
      if (bdot)
        return 1;

      bool adotdot = a[1] == '.' && (alen < 0 ? !a[2] : alen == 2);
      bool bdotdot = b[1] == '.' && (blen < 0 ? !b[2] : blen == 2);
      if (adotdot)
        return -!bdotdot;
      if (bdotdot)
        return 1;
    }
  else if (b[0] == '.')
    return 1;

   
  idx_t aprefixlen = file_prefixlen (a, &alen);
  idx_t bprefixlen = file_prefixlen (b, &blen);

   
  bool one_pass_only = aprefixlen == alen && bprefixlen == blen;

  int result = verrevcmp (a, aprefixlen, b, bprefixlen);

   
  return result || one_pass_only ? result : verrevcmp (a, alen, b, blen);
}
