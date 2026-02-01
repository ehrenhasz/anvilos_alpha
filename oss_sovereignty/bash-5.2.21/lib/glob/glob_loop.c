 

static int INTERNAL_GLOB_PATTERN_P PARAMS((const GCHAR *));

 
static int
INTERNAL_GLOB_PATTERN_P (pattern)
     const GCHAR *pattern;
{
  register const GCHAR *p;
  register GCHAR c;
  int bopen, bsquote;

  p = pattern;
  bopen = bsquote = 0;

  while ((c = *p++) != L('\0'))
    switch (c)
      {
      case L('?'):
      case L('*'):
	return 1;

      case L('['):       
	bopen++;         
	continue;        
      case L(']'):
	if (bopen)
	  return 1;
	continue;

      case L('+'):          
      case L('@'):
      case L('!'):
	if (*p == L('('))   
	  return 1;
	continue;

      case L('\\'):
	 
	if (*p != L('\0'))
	  {
	    p++;
	    bsquote = 1;
	    continue;
	  }
	else 	 
	  return 0;
      }

#if 0
  return bsquote ? 2 : 0;
#else
  return (0);
#endif
}

#undef INTERNAL_GLOB_PATTERN_P
#undef L
#undef INT
#undef CHAR
#undef GCHAR
