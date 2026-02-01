 

 

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#include <stdc.h>

#include <bashansi.h>
#include <bashintl.h>
#include <bashtypes.h>

#include <stdio.h>
#include <ctype.h>
#include <xmalloc.h>

#include <shmbchar.h>
#include <shmbutil.h>
#include <chartypes.h>
#include <typemax.h>

#include <glob/strmatch.h>

#define _to_wupper(wc)	(iswlower (wc) ? towupper (wc) : (wc))
#define _to_wlower(wc)	(iswupper (wc) ? towlower (wc) : (wc))

#if !defined (HANDLE_MULTIBYTE)
#  define cval(s, i, l)	((s)[(i)])
#  define iswalnum(c)	(isalnum(c))
#  define TOGGLE(x)	(ISUPPER (x) ? tolower ((unsigned char)x) : (TOUPPER (x)))
#else
#  define TOGGLE(x)	(iswupper (x) ? towlower (x) : (_to_wupper(x)))
#endif

 
#define CASE_NOOP	0x0000
#define CASE_LOWER	0x0001
#define CASE_UPPER	0x0002
#define CASE_CAPITALIZE	0x0004
#define CASE_UNCAP	0x0008
#define CASE_TOGGLE	0x0010
#define CASE_TOGGLEALL	0x0020
#define CASE_UPFIRST	0x0040
#define CASE_LOWFIRST	0x0080

#define CASE_USEWORDS	0x1000		 

extern char *substring PARAMS((char *, int, int));

#ifndef UCHAR_MAX
#  define UCHAR_MAX	TYPE_MAXIMUM(unsigned char)
#endif

#if defined (HANDLE_MULTIBYTE)
static wchar_t
cval (s, i, l)
     char *s;
     int i, l;
{
  size_t tmp;
  wchar_t wc;
  mbstate_t mps;  

  if (MB_CUR_MAX == 1 || is_basic (s[i]))
    return ((wchar_t)s[i]);
  if (i >= (l - 1))
    return ((wchar_t)s[i]);
  memset (&mps, 0, sizeof (mbstate_t));
  tmp = mbrtowc (&wc, s + i, l - i, &mps);
  if (MB_INVALIDCH (tmp) || MB_NULLWCH (tmp))
    return ((wchar_t)s[i]);
  return wc;  
}
#endif

 
char *
sh_modcase (string, pat, flags)
     const char *string;
     char *pat;
     int flags;
{
  int start, next, end, retind;
  int inword, c, nc, nop, match, usewords;
  char *ret, *s;
  wchar_t wc;
  int mb_cur_max;
#if defined (HANDLE_MULTIBYTE)
  wchar_t nwc;
  char mb[MB_LEN_MAX+1];
  int mlen;
  size_t m;
  mbstate_t state;
#endif

  if (string == 0 || *string == 0)
    {
      ret = (char *)xmalloc (1);
      ret[0] = '\0';
      return ret;
    }

#if defined (HANDLE_MULTIBYTE)
  memset (&state, 0, sizeof (mbstate_t));
#endif

  start = 0;
  end = strlen (string);
  mb_cur_max = MB_CUR_MAX;

  ret = (char *)xmalloc (2*end + 1);
  retind = 0;

   
  usewords = (flags & CASE_USEWORDS);
  flags &= ~CASE_USEWORDS;

  inword = 0;
  while (start < end)
    {
      wc = cval ((char *)string, start, end);

      if (iswalnum (wc) == 0)
	inword = 0;

      if (pat)
	{
	  next = start;
	  ADVANCE_CHAR (string, end, next);
	  s = substring ((char *)string, start, next);
	  match = strmatch (pat, s, FNM_EXTMATCH) != FNM_NOMATCH;
	  free (s);
	  if (match == 0)
            {
               
              memcpy (ret + retind, string + start, next - start);
              retind += next - start;
              start = next;
              inword = 1;
              continue;
            }
	}

       
      if (flags == CASE_CAPITALIZE)
	{
	  if (usewords)
	    nop = inword ? CASE_LOWER : CASE_UPPER;
	  else
	    nop = (start > 0) ? CASE_LOWER : CASE_UPPER;
	  inword = 1;
	}
      else if (flags == CASE_UNCAP)
	{
	  if (usewords)
	    nop = inword ? CASE_UPPER : CASE_LOWER;
	  else
	    nop = (start > 0) ? CASE_UPPER : CASE_LOWER;
	  inword = 1;
	}
      else if (flags == CASE_UPFIRST)
 	{
 	  if (usewords)
	    nop = inword ? CASE_NOOP : CASE_UPPER;
	  else
	    nop = (start > 0) ? CASE_NOOP : CASE_UPPER;
 	  inword = 1;
 	}
      else if (flags == CASE_LOWFIRST)
 	{
 	  if (usewords)
	    nop = inword ? CASE_NOOP : CASE_LOWER;
	  else
	    nop = (start > 0) ? CASE_NOOP : CASE_LOWER;
 	  inword = 1;
 	}
      else if (flags == CASE_TOGGLE)
	{
	  nop = inword ? CASE_NOOP : CASE_TOGGLE;
	  inword = 1;
	}
      else
	nop = flags;

       
      if (mb_cur_max == 1)
	{
singlebyte:
	  switch (nop)
	    {
	    default:
	    case CASE_NOOP:  nc = wc; break;
	    case CASE_UPPER:  nc = TOUPPER (wc); break;
	    case CASE_LOWER:  nc = TOLOWER (wc); break;
	    case CASE_TOGGLEALL:
	    case CASE_TOGGLE: nc = TOGGLE (wc); break;
	    }
	  ret[retind++] = nc;
	}
#if defined (HANDLE_MULTIBYTE)
      else
	{
	  m = mbrtowc (&wc, string + start, end - start, &state);
	   
	  if (MB_INVALIDCH (m))
	    {
	      wc = (unsigned char)string[start];
	      goto singlebyte;
	    }
	  else if (MB_NULLWCH (m))
	    wc = L'\0';
	  switch (nop)
	    {
	    default:
	    case CASE_NOOP:  nwc = wc; break;
	    case CASE_UPPER:  nwc = _to_wupper (wc); break;
	    case CASE_LOWER:  nwc = _to_wlower (wc); break;
	    case CASE_TOGGLEALL:
	    case CASE_TOGGLE: nwc = TOGGLE (wc); break;
	    }

	   
	  if ((int)nwc <= UCHAR_MAX && is_basic ((int)nwc))
	    ret[retind++] = nwc;
	  else
	    {
	      mlen = wcrtomb (mb, nwc, &state);
	      if (mlen > 0)
		mb[mlen] = '\0';
	       
	      strncpy (ret + retind, mb, mlen);
	      retind += mlen;
	    }
	}
#endif

      ADVANCE_CHAR (string, end, start);
    }

  ret[retind] = '\0';
  return ret;
}
