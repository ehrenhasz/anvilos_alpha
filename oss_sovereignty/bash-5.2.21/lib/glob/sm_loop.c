 

extern int interrupt_state, terminating_signal;

struct STRUCT
{
  CHAR *pattern;
  CHAR *string;
};

int FCT PARAMS((CHAR *, CHAR *, int));

static int GMATCH PARAMS((CHAR *, CHAR *, CHAR *, CHAR *, struct STRUCT *, int));
static CHAR *PARSE_COLLSYM PARAMS((CHAR *, INT *));
static CHAR *BRACKMATCH PARAMS((CHAR *, U_CHAR, int));
static int EXTMATCH PARAMS((INT, CHAR *, CHAR *, CHAR *, CHAR *, int));

extern void DEQUOTE_PATHNAME PARAMS((CHAR *));

  CHAR *PATSCAN PARAMS((CHAR *, CHAR *, INT));

int
FCT (pattern, string, flags)
     CHAR *pattern;
     CHAR *string;
     int flags;
{
  CHAR *se, *pe;

  if (string == 0 || pattern == 0)
    return FNM_NOMATCH;

  se = string + STRLEN ((XCHAR *)string);
  pe = pattern + STRLEN ((XCHAR *)pattern);

  return (GMATCH (string, se, pattern, pe, (struct  STRUCT *)NULL, flags));
}

 
static int
GMATCH (string, se, pattern, pe, ends, flags)
     CHAR *string, *se;
     CHAR *pattern, *pe;
     struct STRUCT *ends;
     int flags;
{
  CHAR *p, *n;		 
  INT c;		 
  INT sc;		 

  p = pattern;
  n = string;

  if (string == 0 || pattern == 0)
    return FNM_NOMATCH;

#if DEBUG_MATCHING
fprintf(stderr, "gmatch: string = %s; se = %s\n", string, se);
fprintf(stderr, "gmatch: pattern = %s; pe = %s\n", pattern, pe);
#endif

  while (p < pe)
    {
      c = *p++;
      c = FOLD (c);

      sc = n < se ? *n : '\0';

      if (interrupt_state || terminating_signal)
	return FNM_NOMATCH;

#ifdef EXTENDED_GLOB
       
      if ((flags & FNM_EXTMATCH) && *p == L('(') &&
	  (c == L('+') || c == L('*') || c == L('?') || c == L('@') || c == L('!')))  
	{
	  int lflags;
	   
	  lflags = (n == string) ? flags : (flags & ~(FNM_PERIOD|FNM_DOTDOT));
	  return (EXTMATCH (c, n, se, p, pe, lflags));
	}
#endif  

      switch (c)
	{
	case L('?'):		 
	  if (sc == '\0')
	    return FNM_NOMATCH;
	  else if ((flags & FNM_PATHNAME) && sc == L('/'))
	     
	    return FNM_NOMATCH;
	  else if ((flags & FNM_PERIOD) && sc == L('.') &&
		   (n == string || ((flags & FNM_PATHNAME) && n[-1] == L('/'))))
	     
	    return FNM_NOMATCH;

	   
	  if ((flags & FNM_DOTDOT) &&
	      ((n == string && SDOT_OR_DOTDOT(n)) ||
	       ((flags & FNM_PATHNAME) && n[-1] == L('/') && PDOT_OR_DOTDOT(n))))
	    return FNM_NOMATCH;

	  break;

	case L('\\'):		 
	  if (p == pe && sc == '\\' && (n+1 == se))
	    break;

	  if (p == pe)
	    return FNM_NOMATCH;

	  if ((flags & FNM_NOESCAPE) == 0)
	    {
	      c = *p++;
	       
	      if (p > pe)
		return FNM_NOMATCH;
	      c = FOLD (c);
	    }
	  if (FOLD (sc) != (U_CHAR)c)
	    return FNM_NOMATCH;
	  break;

	case L('*'):		 
	   
	  if (ends != NULL)
	    {
	      ends->pattern = p - 1;
	      ends->string = n;
	      return (0);
	    }

	  if ((flags & FNM_PERIOD) && sc == L('.') &&
	      (n == string || ((flags & FNM_PATHNAME) && n[-1] == L('/'))))
	     
	    return FNM_NOMATCH;

	   
	  if ((flags & FNM_DOTDOT) &&
	      ((n == string && SDOT_OR_DOTDOT(n)) ||
	       ((flags & FNM_PATHNAME) && n[-1] == L('/') && PDOT_OR_DOTDOT(n))))
	    return FNM_NOMATCH;

	  if (p == pe)
	    return 0;

	   
	  for (c = *p++; (c == L('?') || c == L('*')); c = *p++)
	    {
	      if ((flags & FNM_PATHNAME) && sc == L('/'))
		 
		return FNM_NOMATCH;
#ifdef EXTENDED_GLOB
	      else if ((flags & FNM_EXTMATCH) && c == L('?') && *p == L('('))  
		{
		  CHAR *newn;

		   
		  if (EXTMATCH (c, n, se, p, pe, flags) == 0)
		    return (0);

		   
		  newn = PATSCAN (p + 1, pe, 0);
		   
		  p = newn ? newn : pe;
		}
#endif
	      else if (c == L('?'))
		{
		  if (sc == L('\0'))
		    return FNM_NOMATCH;
		   
		  n++;
		  sc = n < se ? *n : '\0';
		}

#ifdef EXTENDED_GLOB
	       
	      if ((flags & FNM_EXTMATCH) && c == L('*') && *p == L('('))   
		{
		  CHAR *newn;
		   
		  for (newn = n; newn < se; ++newn)
		    {
		      if (EXTMATCH (c, newn, se, p, pe, flags) == 0)
			return (0);
		    }
		   
		  newn = PATSCAN (p + 1, pe, 0);
		   
		  p = newn ? newn : pe;
		}
#endif
	      if (p == pe)
		break;
	    }

	   
	  if (c == L('\0'))
	    {
	      int r = (flags & FNM_PATHNAME) == 0 ? 0 : FNM_NOMATCH;
	      if (flags & FNM_PATHNAME)
		{
		  if (flags & FNM_LEADING_DIR)
		    r = 0;
		  else if (MEMCHR (n, L('/'), se - n) == NULL)
		    r = 0;
		}
	      return r;
	    }

	   
	  if (p == pe && (c == L('?') || c == L('*')))
	    return (0);

	   
#if defined (EXTENDED_GLOB)
	  if (n == se && ((flags & FNM_EXTMATCH) && (c == L('!') || c == L('?')) && *p == L('(')))
	    {
	      --p;
	      if (EXTMATCH (c, n, se, p, pe, flags) == 0)
		return (c == L('!') ? FNM_NOMATCH : 0);
	      return (c == L('!') ? 0 : FNM_NOMATCH);
	    }
#endif

	   
	  if (c == L('/') && (flags & FNM_PATHNAME))
	    {
	      while (n < se && *n != L('/'))
		++n;
	      if (n < se && *n == L('/') && (GMATCH (n+1, se, p, pe, NULL, flags) == 0))
		return 0;
	      return FNM_NOMATCH;	 
	    }

	   
	  {
	    U_CHAR c1;
	    const CHAR *endp;
	    struct STRUCT end;

	    end.pattern = NULL;
	    endp = MEMCHR (n, (flags & FNM_PATHNAME) ? L('/') : L('\0'), se - n);
	    if (endp == 0)
	      endp = se;

	    c1 = ((flags & FNM_NOESCAPE) == 0 && c == L('\\')) ? *p : c;
	    c1 = FOLD (c1);
	    for (--p; n < endp; ++n)
	      {
		 
		if ((flags & FNM_EXTMATCH) == 0 && c != L('[') && FOLD (*n) != c1)  
		  continue;

		 
		if ((flags & FNM_EXTMATCH) && p[1] != L('(') &&  
		    STRCHR (L("?*+@!"), *p) == 0 && c != L('[') && FOLD (*n) != c1)  
		  continue;

		 
		if (GMATCH (n, se, p, pe, &end, flags & ~(FNM_PERIOD|FNM_DOTDOT)) == 0)
		  {
		    if (end.pattern == NULL)
		      return (0);
		    break;
		  }
	      }
	      
	    if (end.pattern != NULL)
	      {
		p = end.pattern;
		n = end.string;
		continue;
	      }

	    return FNM_NOMATCH;
	  }

	case L('['):
	  {
	    if (sc == L('\0') || n == se)
	      return FNM_NOMATCH;

	     
	    if ((flags & FNM_PERIOD) && sc == L('.') &&
		(n == string || ((flags & FNM_PATHNAME) && n[-1] == L('/'))))
	      return (FNM_NOMATCH);

	     
	    if ((flags & FNM_DOTDOT) &&
		((n == string && SDOT_OR_DOTDOT(n)) ||
		((flags & FNM_PATHNAME) && n[-1] == L('/') && PDOT_OR_DOTDOT(n))))
	      return FNM_NOMATCH;

	    p = BRACKMATCH (p, sc, flags);
	    if (p == 0)
	      return FNM_NOMATCH;
	  }
	  break;

	default:
	  if ((U_CHAR)c != FOLD (sc))
	    return (FNM_NOMATCH);
	}

      ++n;
    }

  if (n == se)
    return (0);

  if ((flags & FNM_LEADING_DIR) && *n == L('/'))
     
    return 0;
	  
  return (FNM_NOMATCH);
}

 
static CHAR *
PARSE_COLLSYM (p, vp)
     CHAR *p;
     INT *vp;
{
  register int pc;
  INT val;

  p++;				 
	  
  for (pc = 0; p[pc]; pc++)
    if (p[pc] == L('.') && p[pc+1] == L(']'))
      break;
   if (p[pc] == 0)
    {
      if (vp)
	*vp = INVALID;
      return (p + pc);
    }
   val = COLLSYM (p, pc);
   if (vp)
     *vp = val;
   return (p + pc + 2);
}

 
static CHAR *
#if defined (PROTOTYPES)
BRACKMATCH (CHAR *p, U_CHAR test, int flags)
#else
BRACKMATCH (p, test, flags)
     CHAR *p;
     U_CHAR test;
     int flags;
#endif
{
  register CHAR cstart, cend, c;
  register int not;     
  int brcnt, forcecoll, isrange;
  INT pc;
  CHAR *savep;
  CHAR *brchrp;
  U_CHAR orig_test;

  orig_test = test;
  test = FOLD (orig_test);

  savep = p;

   
  if (not = (*p == L('!') || *p == L('^')))
    ++p;

  c = *p++;
  for (;;)
    {
       
      cstart = cend = c;
      forcecoll = 0;

       
      if (c == L('[') && *p == L('=') && p[2] == L('=') && p[3] == L(']'))
	{
	  pc = FOLD (p[1]);
	  p += 4;
	  if (COLLEQUIV (test, pc))
	    {
 	       
	      p++;
	      goto matched;
	    }
	  else
	    {
	      c = *p++;
	      if (c == L('\0'))
		return ((test == L('[')) ? savep : (CHAR *)0);  
	      c = FOLD (c);
	      continue;
	    }
	}

       
      if (c == L('[') && *p == L(':'))
	{
	  CHAR *close, *ccname;

	  pc = 0;	 
	   
	  for (close = p + 1; *close != '\0'; close++)
	    if (*close == L(':') && *(close+1) == L(']'))
	      break;

	  if (*close != L('\0'))
	    {
	      ccname = (CHAR *)malloc ((close - p) * sizeof (CHAR));
	      if (ccname == 0)
		pc = 0;
	      else
		{
		  bcopy (p + 1, ccname, (close - p - 1) * sizeof (CHAR));
		  *(ccname + (close - p - 1)) = L('\0');
		   
		  DEQUOTE_PATHNAME (ccname);
		  pc = IS_CCLASS (orig_test, (XCHAR *)ccname);
		}
	      if (pc == -1)
		{
		   
		  pc = 0;
		  p = close + 2;
		}
	      else
		p = close + 2;		 

	      free (ccname);
	    }
	    
	  if (pc)
	    {
 	       
	      p++;
	      goto matched;
	    }
	  else
	    {
	       
	      c = *p++;
	      if (c == L('\0'))
		return ((test == L('[')) ? savep : (CHAR *)0);
	      else if (c == L(']'))
		break;
	      c = FOLD (c);
	      continue;
	    }
	}
 
       
      if (c == L('[') && *p == L('.'))
	{
	  p = PARSE_COLLSYM (p, &pc);
	   
	  cstart = (pc == INVALID) ? test + 1 : pc;
	  forcecoll = 1;
	}

      if (!(flags & FNM_NOESCAPE) && c == L('\\'))
	{
	  if (*p == '\0')
	    return (CHAR *)0;
	  cstart = cend = *p++;
	}

      cstart = cend = FOLD (cstart);
      isrange = 0;

       
      if (c == L('\0'))
	return ((test == L('[')) ? savep : (CHAR *)0);

      c = *p++;
      c = FOLD (c);

      if (c == L('\0'))
	return ((test == L('[')) ? savep : (CHAR *)0);

      if ((flags & FNM_PATHNAME) && c == L('/'))
	 
	return (CHAR *)0;

       
      if (c == L('-') && *p != L(']'))
	{
	  cend = *p++;
	  if (!(flags & FNM_NOESCAPE) && cend == L('\\'))
	    cend = *p++;
	  if (cend == L('\0'))
	    return (CHAR *)0;
	  if (cend == L('[') && *p == L('.'))
	    {
	      p = PARSE_COLLSYM (p, &pc);
	       
	      cend = (pc == INVALID) ? test - 1 : pc;
	      forcecoll = 1;
	    }
	  cend = FOLD (cend);

	  c = *p++;

	   
	  if (RANGECMP (cstart, cend, forcecoll) > 0)
	    {
	      if (c == L(']'))
		break;
	      c = FOLD (c);
	      continue;
	    }
	  isrange = 1;
	}

      if (isrange == 0 && test == cstart)
        goto matched;
      if (isrange && RANGECMP (test, cstart, forcecoll) >= 0 && RANGECMP (test, cend, forcecoll) <= 0)
	goto matched;

      if (c == L(']'))
	break;
    }
   
  return (!not ? (CHAR *)0 : p);

matched:
   
  c = *--p;
  brcnt = 1;
  brchrp = 0;
  while (brcnt > 0)
    {
      int oc;

       
      if (c == L('\0'))
	return ((test == L('[')) ? savep : (CHAR *)0);

      oc = c;
      c = *p++;
      if (c == L('[') && (*p == L('=') || *p == L(':') || *p == L('.')))
	{
	  brcnt++;
	  brchrp = p++;		 
	  if ((c = *p) == L('\0'))
	    return ((test == L('[')) ? savep : (CHAR *)0);
	   
	}
       
      else if (c == L(']') && brcnt > 1 && brchrp != 0 && oc == *brchrp)
	{
	  brcnt--;
	  brchrp = 0;		 
	}
       
      else if (c == L(']') && (brchrp == 0 || *brchrp != L('.')) && brcnt >= 1)
	brcnt = 0;
      else if (!(flags & FNM_NOESCAPE) && c == L('\\'))
	{
	  if (*p == '\0')
	    return (CHAR *)0;
	   
	  ++p;
	}
    }
  return (not ? (CHAR *)0 : p);
}

#if defined (EXTENDED_GLOB)
 

 
  CHAR *
PATSCAN (string, end, delim)
     CHAR *string, *end;
     INT delim;
{
  int pnest, bnest, skip;
  INT cchar;
  CHAR *s, c, *bfirst;

  pnest = bnest = skip = 0;
  cchar = 0;
  bfirst = NULL;

  if (string == end)
    return (NULL);

  for (s = string; c = *s; s++)
    {
      if (s >= end)
	return (s);
      if (skip)
	{
	  skip = 0;
	  continue;
	}
      switch (c)
	{
	case L('\\'):
	  skip = 1;
	  break;

	case L('\0'):
	  return ((CHAR *)NULL);

	 
	case L('['):
	  if (bnest == 0)
	    {
	      bfirst = s + 1;
	      if (*bfirst == L('!') || *bfirst == L('^'))
		bfirst++;
	      bnest++;
	    }
	  else if (s[1] == L(':') || s[1] == L('.') || s[1] == L('='))
	    cchar = s[1];
	  break;

	 
	case L(']'):
	  if (bnest)
	    {
	      if (cchar && s[-1] == cchar)
		cchar = 0;
	      else if (s != bfirst)
		{
		  bnest--;
		  bfirst = 0;
		}
	    }
	  break;

	case L('('):
	  if (bnest == 0)
	    pnest++;
	  break;

	case L(')'):
	  if (bnest == 0 && pnest-- <= 0)
	    return ++s;
	  break;

	case L('|'):
	  if (bnest == 0 && pnest == 0 && delim == L('|'))
	    return ++s;
	  break;
	}
    }

  return (NULL);
}

 
static int
STRCOMPARE (p, pe, s, se)
     CHAR *p, *pe, *s, *se;
{
  int ret;
  CHAR c1, c2;
  int l1, l2;

  l1 = pe - p;
  l2 = se - s;

  if (l1 != l2)
    return (FNM_NOMATCH);	 
  
  c1 = *pe;
  c2 = *se;

  if (c1 != 0)
    *pe = '\0';
  if (c2 != 0)
    *se = '\0';
    
#if HAVE_MULTIBYTE || defined (HAVE_STRCOLL)
  ret = STRCOLL ((XCHAR *)p, (XCHAR *)s);
#else
  ret = STRCMP ((XCHAR *)p, (XCHAR *)s);
#endif

  if (c1 != 0)
    *pe = c1;
  if (c2 != 0)
    *se = c2;

  return (ret == 0 ? ret : FNM_NOMATCH);
}

 
static int
EXTMATCH (xc, s, se, p, pe, flags)
     INT xc;		 
     CHAR *s, *se;
     CHAR *p, *pe;
     int flags;
{
  CHAR *prest;			 
  CHAR *psub;			 
  CHAR *pnext;			 
  CHAR *srest;			 
  int m1, m2, xflags;		 

#if DEBUG_MATCHING
fprintf(stderr, "extmatch: xc = %c\n", xc);
fprintf(stderr, "extmatch: s = %s; se = %s\n", s, se);
fprintf(stderr, "extmatch: p = %s; pe = %s\n", p, pe);
fprintf(stderr, "extmatch: flags = %d\n", flags);
#endif

  prest = PATSCAN (p + (*p == L('(')), pe, 0);  
  if (prest == 0)
     
    return (STRCOMPARE (p - 1, pe, s, se));

  switch (xc)
    {
    case L('+'):		 
    case L('*'):		 
       
      if (xc == L('*') && (GMATCH (s, se, prest, pe, NULL, flags) == 0))
	return 0;

       
      for (psub = p + 1; ; psub = pnext)
	{
	  pnext = PATSCAN (psub, pe, L('|'));
	  for (srest = s; srest <= se; srest++)
	    {
	       
	      m1 = GMATCH (s, srest, psub, pnext - 1, NULL, flags) == 0;
	       
	      if (m1)
		{
		   
		  xflags = (srest > s) ? (flags & ~(FNM_PERIOD|FNM_DOTDOT)) : flags;
		  m2 = (GMATCH (srest, se, prest, pe, NULL, xflags) == 0) ||
			(s != srest && GMATCH (srest, se, p - 1, pe, NULL, xflags) == 0);
		}
	      if (m1 && m2)
		return (0);
	    }
	  if (pnext == prest)
	    break;
	}
      return (FNM_NOMATCH);

    case L('?'):		 
    case L('@'):		 
       
      if (xc == L('?') && (GMATCH (s, se, prest, pe, NULL, flags) == 0))
	return 0;

       
      for (psub = p + 1; ; psub = pnext)
	{
	  pnext = PATSCAN (psub, pe, L('|'));
	  srest = (prest == pe) ? se : s;
	  for ( ; srest <= se; srest++)
	    {
	       
	      xflags = (srest > s) ? (flags & ~(FNM_PERIOD|FNM_DOTDOT)) : flags;
	      if (GMATCH (s, srest, psub, pnext - 1, NULL, flags) == 0 &&
		  GMATCH (srest, se, prest, pe, NULL, xflags) == 0)
		return (0);
	    }
	  if (pnext == prest)
	    break;
	}
      return (FNM_NOMATCH);

    case '!':		 
      for (srest = s; srest <= se; srest++)
	{
	  m1 = 0;
	  for (psub = p + 1; ; psub = pnext)
	    {
	      pnext = PATSCAN (psub, pe, L('|'));
	       
	      if (m1 = (GMATCH (s, srest, psub, pnext - 1, NULL, flags) == 0))
		break;
	      if (pnext == prest)
		break;
	    }

	   
	  if (m1 == 0 && (flags & FNM_PERIOD) && *s == '.')
	    return (FNM_NOMATCH);

	  if (m1 == 0 && (flags & FNM_DOTDOT) &&
	      (SDOT_OR_DOTDOT (s) ||
	       ((flags & FNM_PATHNAME) && s[-1] == L('/') && PDOT_OR_DOTDOT(s))))
	    return (FNM_NOMATCH);

	   
	  xflags = (srest > s) ? (flags & ~(FNM_PERIOD|FNM_DOTDOT)) : flags;
	  if (m1 == 0 && GMATCH (srest, se, prest, pe, NULL, xflags) == 0)
	    return (0);
	}
      return (FNM_NOMATCH);
    }

  return (FNM_NOMATCH);
}
#endif  

#undef IS_CCLASS
#undef FOLD
#undef CHAR
#undef U_CHAR
#undef XCHAR
#undef INT
#undef INVALID
#undef FCT
#undef GMATCH
#undef COLLSYM
#undef PARSE_COLLSYM
#undef PATSCAN
#undef STRCOMPARE
#undef EXTMATCH
#undef DEQUOTE_PATHNAME
#undef STRUCT
#undef BRACKMATCH
#undef STRCHR
#undef STRCOLL
#undef STRLEN
#undef STRCMP
#undef MEMCHR
#undef COLLEQUIV
#undef RANGECMP
#undef ISDIRSEP
#undef PATHSEP
#undef PDOT_OR_DOTDOT
#undef SDOT_OR_DOTDOT
#undef L
