 
const unsigned int is_basic_table [UCHAR_MAX / 32 + 1] =
{
  0x00001a00,            
  0xffffffef,            
  0xfffffffe,            
  0x7ffffffe             
   
};

#endif  

extern int locale_utf8locale;

extern char *utf8_mbsmbchar (const char *);
extern int utf8_mblen (const char *, size_t);

 
size_t
mbstrlen (s)
     const char *s;
{
  size_t clen, nc;
  mbstate_t mbs = { 0 }, mbsbak = { 0 };
  int f, mb_cur_max;

  nc = 0;
  mb_cur_max = MB_CUR_MAX;
  while (*s && (clen = (f = is_basic (*s)) ? 1 : mbrlen(s, mb_cur_max, &mbs)) != 0)
    {
      if (MB_INVALIDCH(clen))
	{
	  clen = 1;	 
	  mbs = mbsbak;
	}

      if (f == 0)
	mbsbak = mbs;

      s += clen;
      nc++;
    }
  return nc;
}

 
 
char *
mbsmbchar (s)
     const char *s;
{
  char *t;
  size_t clen;
  mbstate_t mbs = { 0 };
  int mb_cur_max;

  if (locale_utf8locale)
    return (utf8_mbsmbchar (s));	 

  mb_cur_max = MB_CUR_MAX;
  for (t = (char *)s; *t; t++)
    {
      if (is_basic (*t))
	continue;

      if (locale_utf8locale)		 
	clen = utf8_mblen (t, mb_cur_max);
      else
	clen = mbrlen (t, mb_cur_max, &mbs);

      if (clen == 0)
        return 0;
      if (MB_INVALIDCH(clen))
	continue;

      if (clen > 1)
	return t;
    }
  return 0;
}

int
sh_mbsnlen(src, srclen, maxlen)
     const char *src;
     size_t srclen;
     int maxlen;
{
  int count;
  int sind;
  DECLARE_MBSTATE;

  for (sind = count = 0; src[sind]; )
    {
      count++;		 
      ADVANCE_CHAR (src, srclen, sind);
      if (sind > maxlen)
        break;
    }

  return count;
}
#endif
