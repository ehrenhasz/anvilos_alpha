 

 

 
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif

#include <config.h>

#include <bashansi.h>

 
#include <shmbutil.h>

#if HANDLE_MULTIBYTE

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif

#define WSBUF_INC 32

#ifndef FREE
#  define FREE(x)	do { if (x) free (x); } while (0)
#endif

#if ! HAVE_STRCHRNUL
extern char *strchrnul PARAMS((const char *, int));
#endif

 

static mbstate_t local_state;
static int local_state_use = 0;

size_t
xmbsrtowcs (dest, src, len, pstate)
    wchar_t *dest;
    const char **src;
    size_t len;
    mbstate_t *pstate;
{
  mbstate_t *ps;
  size_t mblength, wclength, n;

  ps = pstate;
  if (pstate == NULL)
    {
      if (!local_state_use)
	{
	  memset (&local_state, '\0', sizeof(mbstate_t));
	  local_state_use = 1;
	}
      ps = &local_state;
    }

  n = strlen (*src);

  if (dest == NULL)
    {
      wchar_t *wsbuf;
      const char *mbs;
      mbstate_t psbuf;

       
      wsbuf = (wchar_t *) malloc ((n + 1) * sizeof(wchar_t));
      mbs = *src;
      psbuf = *ps;

      wclength = mbsrtowcs (wsbuf, &mbs, n, &psbuf);

      if (wsbuf)
	free (wsbuf);
      return wclength;
    }
      
  for (wclength = 0; wclength < len; wclength++, dest++)
    {
      if (mbsinit(ps))
	{
	  if (**src == '\0')
	    {
	      *dest = L'\0';
	      *src = NULL;
	      return (wclength);
	    }
	  else if (**src == '\\')
	    {
	      *dest = L'\\';
	      mblength = 1;
	    }
	  else
	    mblength = mbrtowc(dest, *src, n, ps);
	}
      else
	mblength = mbrtowc(dest, *src, n, ps);

       
      if (mblength == (size_t)-1 || mblength == (size_t)-2)
	return (size_t)-1;

      *src += mblength;
      n -= mblength;

       
      if (*dest == L'\0')
	{
	  *src = NULL;
	  break;
	}
    }

    return (wclength);
}

#if HAVE_MBSNRTOWCS
 

static size_t
xdupmbstowcs2 (destp, src)
    wchar_t **destp;	 
    const char *src;	 
{
  const char *p;	 
  wchar_t *wsbuf;	 
  size_t wsbuf_size;	 
  size_t wcnum;		 
  mbstate_t state;	 
  size_t n, wcslength;	 
  const char *end_or_backslash;
  size_t nms;	 
  mbstate_t tmp_state;
  const char *tmp_p;

  memset (&state, '\0', sizeof(mbstate_t));

  wsbuf_size = 0;
  wsbuf = NULL;

  p = src;
  wcnum = 0;
  do
    {
      end_or_backslash = strchrnul(p, '\\');
      nms = end_or_backslash - p;
      if (*end_or_backslash == '\0')
	nms++;

       
      tmp_p = p;
      tmp_state = state;

      if (nms == 0 && *p == '\\')	 
	nms = wcslength = 1;
      else
	wcslength = mbsnrtowcs (NULL, &tmp_p, nms, 0, &tmp_state);

      if (wcslength == 0)
	{
	  tmp_p = p;		 
	  tmp_state = state;
	  wcslength = 1;	 
	}

       
      if (wcslength == (size_t)-1)
	{
	  free (wsbuf);
	  *destp = NULL;
	  return (size_t)-1;
	}

       
      if (wsbuf_size < wcnum+wcslength+1)	 
	{
	  wchar_t *wstmp;

	  while (wsbuf_size < wcnum+wcslength+1)  
	    wsbuf_size += WSBUF_INC;

	  wstmp = (wchar_t *) realloc (wsbuf, wsbuf_size * sizeof (wchar_t));
	  if (wstmp == NULL)
	    {
	      free (wsbuf);
	      *destp = NULL;
	      return (size_t)-1;
	    }
	  wsbuf = wstmp;
	}

       
      n = mbsnrtowcs(wsbuf+wcnum, &p, nms, wsbuf_size-wcnum, &state);

      if (n == 0 && p == 0)
	{
	  wsbuf[wcnum] = L'\0';
	  break;
	}

       
      if (wcslength == 1 && (n == 0 || n == (size_t)-1))
	{
	  state = tmp_state;
	  p = tmp_p;
	  wsbuf[wcnum] = *p;
	  if (*p == 0)
	    break;
	  else
	    {
	      wcnum++; p++;
	    }
	}
      else
        wcnum += wcslength;

      if (mbsinit (&state) && (p != NULL) && (*p == '\\'))
	{
	  wsbuf[wcnum++] = L'\\';
	  p++;
	}
    }
  while (p != NULL);

  *destp = wsbuf;

   
  return wcnum;
}
#endif  

 

size_t
xdupmbstowcs (destp, indicesp, src)
    wchar_t **destp;	 
    char ***indicesp;	 
    const char *src;	 
{
  const char *p;	 
  wchar_t wc;		 
  wchar_t *wsbuf;	 
  char **indices; 	 
  size_t wsbuf_size;	 
  size_t wcnum;		 
  mbstate_t state;	 

   
  if (src == NULL || destp == NULL)
    {
      if (destp)
	*destp = NULL;
      if (indicesp)
	*indicesp = NULL;
      return (size_t)-1;
    }

#if HAVE_MBSNRTOWCS
  if (indicesp == NULL)
    return (xdupmbstowcs2 (destp, src));
#endif

  memset (&state, '\0', sizeof(mbstate_t));
  wsbuf_size = WSBUF_INC;

  wsbuf = (wchar_t *) malloc (wsbuf_size * sizeof(wchar_t));
  if (wsbuf == NULL)
    {
      *destp = NULL;
      if (indicesp)
        *indicesp = NULL;
      return (size_t)-1;
    }

  indices = NULL;
  if (indicesp)
    {
      indices = (char **) malloc (wsbuf_size * sizeof(char *));
      if (indices == NULL)
	{
	  free (wsbuf);
	  *destp = NULL;
	  *indicesp = NULL;
	  return (size_t)-1;
	}
    }

  p = src;
  wcnum = 0;
  do
    {
      size_t mblength;	 

      if (mbsinit (&state))
	{
	  if (*p == '\0')
	    {
	      wc = L'\0';
	      mblength = 1;
	    }
	  else if (*p == '\\')
	    {
	      wc = L'\\';
	      mblength = 1;
	    }
	  else
	    mblength = mbrtowc(&wc, p, MB_LEN_MAX, &state);
	}
      else
	mblength = mbrtowc(&wc, p, MB_LEN_MAX, &state);

       
      if (MB_INVALIDCH (mblength))
	{
	  free (wsbuf);
	  FREE (indices);
	  *destp = NULL;
	  if (indicesp)
	    *indicesp = NULL;
	  return (size_t)-1;
	}

      ++wcnum;

       
      if (wsbuf_size < wcnum)
	{
	  wchar_t *wstmp;
	  char **idxtmp;

	  wsbuf_size += WSBUF_INC;

	  wstmp = (wchar_t *) realloc (wsbuf, wsbuf_size * sizeof (wchar_t));
	  if (wstmp == NULL)
	    {
	      free (wsbuf);
	      FREE (indices);
	      *destp = NULL;
	      if (indicesp)
		*indicesp = NULL;
	      return (size_t)-1;
	    }
	  wsbuf = wstmp;

	  if (indicesp)
	    {
	      idxtmp = (char **) realloc (indices, wsbuf_size * sizeof (char *));
	      if (idxtmp == NULL)
		{
		  free (wsbuf);
		  free (indices);
		  *destp = NULL;
		  if (indicesp)
		    *indicesp = NULL;
		  return (size_t)-1;
		}
	      indices = idxtmp;
	    }
	}

      wsbuf[wcnum - 1] = wc;
      if (indices)
        indices[wcnum - 1] = (char *)p;
      p += mblength;
    }
  while (MB_NULLWCH (wc) == 0);

   
  *destp = wsbuf;
  if (indicesp != NULL)
    *indicesp = indices;

  return (wcnum - 1);
}

 

 
size_t
xwcsrtombs (char *dest, const wchar_t **srcp, size_t len, mbstate_t *ps)
{
  const wchar_t *src;
  size_t cur_max;			 
  char buf[64], *destptr, *tmp_dest;
  unsigned char uc;
  mbstate_t prev_state;

  cur_max = MB_CUR_MAX;
  if (cur_max > sizeof (buf))		 
    return (size_t)-1;

  src = *srcp;

  if (dest != NULL)
    {
      destptr = dest;

      for (; len > 0; src++)
	{
	  wchar_t wc;
	  size_t ret;

	  wc = *src;
	   
	  tmp_dest = destptr;
	  ret = wcrtomb (len >= cur_max ? destptr : buf, wc, ps);

	  if (ret == (size_t)(-1))		 
	    {
	       
handle_byte:
	      destptr = tmp_dest;	 
	      uc = wc;
	      ret = 1;
	      if (len >= cur_max)
		*destptr = uc;
	      else
		buf[0] = uc;
	      if (ps)
		memset (ps, 0, sizeof (mbstate_t));
	    }

	  if (ret > cur_max)		 
	    goto bad_input;

	  if (len < ret)
	    break;

	  if (len < cur_max)
	    memcpy (destptr, buf, ret);

	  if (wc == 0)
	    {
	      src = NULL;
	       
	      break;
	    }
	  destptr += ret;
	  len -= ret;
	}
      *srcp = src;
      return destptr - dest;
    }
  else
    {
       
      mbstate_t state = *ps;
      size_t totalcount = 0;

      for (;; src++)
	{
	  wchar_t wc;
	  size_t ret;

	  wc = *src;
	  ret = wcrtomb (buf, wc, &state);

	  if (ret == (size_t)(-1))
	    goto bad_input2;
	  if (wc == 0)
	    {
	       
	      break;
	    }
	  totalcount += ret;
	}
      return totalcount;
    }

bad_input:
  *srcp = src;
bad_input2:
  errno = EILSEQ;
  return (size_t)(-1);
}

#endif  
