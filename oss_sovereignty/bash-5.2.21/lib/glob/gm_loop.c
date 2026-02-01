 

#if EXTENDED_GLOB
int
EXTGLOB_PATTERN_P (pat)
     const CHAR *pat;
{
  switch (pat[0])
    {
    case L('*'):
    case L('+'):
    case L('!'):
    case L('@'):
    case L('?'):
      return (pat[1] == L('('));	 
    default:
      return 0;
    }
    
  return 0;
}
#endif

 
int
MATCH_PATTERN_CHAR (pat, string, flags)
     CHAR *pat, *string;
     int flags;
{
  CHAR c;

  if (*string == 0)
    return (*pat == L('*'));	 

  switch (c = *pat++)
    {
    default:
      return (FOLD(*string) == FOLD(c));
    case L('\\'):
      return (FOLD(*string) == FOLD(*pat));
    case L('?'):
      return (*pat == L('(') ? 1 : (*string != L'\0'));
    case L('*'):
      return (1);
    case L('+'):
    case L('!'):
    case L('@'):
      return (*pat ==  L('(') ? 1 : (FOLD(*string) == FOLD(c)));
    case L('['):
      return (*string != L('\0'));
    }
}

int
MATCHLEN (pat, max)
     CHAR *pat;
     size_t max;
{
  CHAR c;
  int matlen, bracklen, t, in_cclass, in_collsym, in_equiv;

  if (*pat == 0)
    return (0);

  matlen = in_cclass = in_collsym = in_equiv = 0;
  while (c = *pat++)
    {
      switch (c)
	{
	default:
	  matlen++;
	  break;
	case L('\\'):
	  if (*pat == 0)
	    return ++matlen;
	  else
	    {
	      matlen++;
	      pat++;
	    }
	  break;
	case L('?'):
	  if (*pat == LPAREN)
	    return (matlen = -1);		 
	  else
	    matlen++;
	  break;
	case L('*'):
	  return (matlen = -1);
	case L('+'):
	case L('!'):
	case L('@'):
	  if (*pat == LPAREN)
	    return (matlen = -1);		 
	  else
	    matlen++;
	  break;
	case L('['):
	   
	  bracklen = 1;
	  c = *pat++;
	  do
	    {
	      if (c == 0)
		{
		  pat--;			 
	          matlen += bracklen;
	          goto bad_bracket;
	        }
	      else if (c == L('\\'))
		{
		   
		  bracklen++;
		   
		  if (*pat == 0 || *++pat == 0)
		    {
		      matlen += bracklen;
		      goto bad_bracket;
		    }
		}
	      else if (c == L('[') && *pat == L(':'))	 
		{
		  pat++;
		  bracklen++;
		  in_cclass = 1;
		}
	      else if (in_cclass && c == L(':') && *pat == L(']'))
		{
		  pat++;
		  bracklen++;
		  in_cclass = 0;
		}
	      else if (c == L('[') && *pat == L('.'))	 
		{
		  pat++;
		  bracklen++;
		  if (*pat == L(']'))	 
		    {
		      pat++;
		      bracklen++;
		    }
		  in_collsym = 1;
		}
	      else if (in_collsym && c == L('.') && *pat == L(']'))
		{
		  pat++;
		  bracklen++;
		  in_collsym = 0;
		}
	      else if (c == L('[') && *pat == L('='))	 
		{
		  pat++;
		  bracklen++;
		  if (*pat == L(']'))	 
		    {
		      pat++;
		      bracklen++;
		    }
		  in_equiv = 1;
		}
	      else if (in_equiv && c == L('=') && *pat == L(']'))
		{
		  pat++;
		  bracklen++;
		  in_equiv = 0;
		}
	      else
		bracklen++;
	    }
	  while ((c = *pat++) != L(']'));
	  matlen++;		 
bad_bracket:
	  break;
	}
    }

  return matlen;
}

#undef EXTGLOB_PATTERN_P
#undef MATCH_PATTERN_CHAR
#undef MATCHLEN
#undef FOLD
#undef L
#undef LPAREN
#undef RPAREN
#undef INT
#undef CHAR
