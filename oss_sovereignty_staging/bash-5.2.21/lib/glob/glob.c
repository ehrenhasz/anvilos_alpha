 

 

#include <config.h>

#if !defined (__GNUC__) && !defined (HAVE_ALLOCA_H) && defined (_AIX)
  #pragma alloca
#endif  

#include "bashtypes.h"

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include "bashansi.h"
#include "posixdir.h"
#include "posixstat.h"
#include "shmbutil.h"
#include "xmalloc.h"

#include "filecntl.h"
#if !defined (F_OK)
#  define F_OK 0
#endif

#include "stdc.h"
#include "memalloc.h"

#include <signal.h>

#include "shell.h"
#include "general.h"

#include "glob.h"
#include "strmatch.h"

#if !defined (HAVE_BCOPY) && !defined (bcopy)
#  define bcopy(s, d, n) ((void) memcpy ((d), (s), (n)))
#endif  

#if !defined (NULL)
#  if defined (__STDC__)
#    define NULL ((void *) 0)
#  else
#    define NULL 0x0
#  endif  
#endif  

#if !defined (FREE)
#  define FREE(x)	if (x) free (x)
#endif

 
#ifndef ALLOCA_MAX
#  define ALLOCA_MAX	100000
#endif

struct globval
  {
    struct globval *next;
    char *name;
  };

extern void throw_to_top_level PARAMS((void));
extern int sh_eaccess PARAMS((const char *, int));
extern char *sh_makepath PARAMS((const char *, const char *, int));
extern int signal_is_pending PARAMS((int));
extern void run_pending_traps PARAMS((void));

extern int extended_glob;

 
int noglob_dot_filenames = 1;

 
int glob_ignore_case = 0;

 
int glob_always_skip_dot_and_dotdot = 1;

 
char *glob_error_return;

static struct globval finddirs_error_return;

 
static int skipname PARAMS((char *, char *, int));
#if HANDLE_MULTIBYTE
static int mbskipname PARAMS((char *, char *, int));
#endif
void udequote_pathname PARAMS((char *));
#if HANDLE_MULTIBYTE
void wcdequote_pathname PARAMS((wchar_t *));
static void wdequote_pathname PARAMS((char *));
static void dequote_pathname PARAMS((char *));
#else
#  define dequote_pathname(p) udequote_pathname(p)
#endif
static int glob_testdir PARAMS((char *, int));
static char **glob_dir_to_array PARAMS((char *, char **, int));

 
extern char *glob_patscan PARAMS((char *, char *, int));
extern wchar_t *glob_patscan_wc PARAMS((wchar_t *, wchar_t *, int));

 
extern int wextglob_pattern_p PARAMS((wchar_t *));

extern char *glob_dirscan PARAMS((char *, int));

 
#define GCHAR	unsigned char
#define CHAR	char
#define INT	int
#define L(CS)	CS
#define INTERNAL_GLOB_PATTERN_P internal_glob_pattern_p
#include "glob_loop.c"

 
#if HANDLE_MULTIBYTE

#define GCHAR	wchar_t
#define CHAR	wchar_t
#define INT	wint_t
#define L(CS)	L##CS
#define INTERNAL_GLOB_PATTERN_P internal_glob_wpattern_p
#include "glob_loop.c"

#endif  

 
int
glob_pattern_p (pattern)
     const char *pattern;
{
#if HANDLE_MULTIBYTE
  size_t n;
  wchar_t *wpattern;
  int r;

  if (MB_CUR_MAX == 1 || mbsmbchar (pattern) == 0)
    return (internal_glob_pattern_p ((unsigned char *)pattern));

   
  n = xdupmbstowcs (&wpattern, NULL, pattern);
  if (n == (size_t)-1)
     
    return (internal_glob_pattern_p ((unsigned char *)pattern));

  r = internal_glob_wpattern_p (wpattern);
  free (wpattern);

  return r;
#else
  return (internal_glob_pattern_p ((unsigned char *)pattern));
#endif
}

#if EXTENDED_GLOB

#if defined (HANDLE_MULTIBYTE)
#  define XSKIPNAME(p, d, f)	mbskipname(p, d, f)
#else
#  define XSKIPNAME(p, d, f)	skipname(p, d, f)
#endif

 
static int
extglob_skipname (pat, dname, flags)
     char *pat, *dname;
     int flags;
{
  char *pp, *pe, *t, *se;
  int n, r, negate, wild, nullpat, xflags;

  negate = *pat == '!';
  wild = *pat == '*' || *pat == '?';
  pp = pat + 2;
  se = pp + strlen (pp);		 
  pe = glob_patscan (pp, se, 0);	 

   
  if (pe == 0)
    return 0;

  xflags = flags | ( negate ? GX_NEGATE : 0);

   
  if (pe == se && *pe == 0 && pe[-1] == ')' && (t = strchr (pp, '|')) == 0)
    {
      pe[-1] = '\0';
       
      r = XSKIPNAME (pp, dname, xflags);  
      pe[-1] = ')';
      return r;
    }

   
  nullpat = pe >= (pat + 2) && pe[-2] == '(' && pe[-1] == ')';

   
  while (t = glob_patscan (pp, pe, '|'))
    {
       
      n = t[-1];	 
      if (extglob_pattern_p (pp) && n == ')')		 
	t[-1] = n;	 
      else
	t[-1] = '\0';
      r = XSKIPNAME (pp, dname, xflags);
      t[-1] = n;
      if (r == 0)	 
        return r;
      pp = t;
      if (pp == pe)
	break;
    }

   
  if (pp == se)
    return r;

   
  if (wild && *pe)	 
    return (XSKIPNAME (pe, dname, flags));

  return 1;
}
#endif

 
static int
skipname (pat, dname, flags)
     char *pat;
     char *dname;
     int flags;
{
  int i;

#if EXTENDED_GLOB
  if (extglob_pattern_p (pat))		 
    return (extglob_skipname (pat, dname, flags));
#endif

  if (glob_always_skip_dot_and_dotdot && DOT_OR_DOTDOT (dname))
    return 1;

   
  if (noglob_dot_filenames == 0 && pat[0] != '.' &&
	(pat[0] != '\\' || pat[1] != '.') &&
	DOT_OR_DOTDOT (dname))
    return 1;

#if 0
   
  else if ((flags & GX_NEGATE) && noglob_dot_filenames == 0 &&
	dname[0] == '.' &&
	(pat[0] == '.' || (pat[0] == '\\' && pat[1] == '.')))
    return 0;
#endif

   
  else if (noglob_dot_filenames && dname[0] == '.' &&
 	   pat[0] != '.' && (pat[0] != '\\' || pat[1] != '.'))
    return 1;

  return 0;
}

#if HANDLE_MULTIBYTE

static int
wskipname (pat, dname, flags)
     wchar_t *pat, *dname;
     int flags;
{
  int i;

  if (glob_always_skip_dot_and_dotdot && WDOT_OR_DOTDOT (dname))
    return 1;

   
  if (noglob_dot_filenames == 0 && pat[0] != L'.' &&
	(pat[0] != L'\\' || pat[1] != L'.') &&
	WDOT_OR_DOTDOT (dname))
    return 1;

#if 0
   
  else if ((flags & GX_NEGATE) && noglob_dot_filenames == 0 &&
	dname[0] == L'.' &&
	(pat[0] == L'.' || (pat[0] == L'\\' && pat[1] == L'.')))
    return 0;
#endif

   
  else if (noglob_dot_filenames && dname[0] == L'.' &&
	pat[0] != L'.' && (pat[0] != L'\\' || pat[1] != L'.'))
    return 1;

  return 0;
}

static int
wextglob_skipname (pat, dname, flags)
     wchar_t *pat, *dname;
     int flags;
{
#if EXTENDED_GLOB
  wchar_t *pp, *pe, *t, *se, n;
  int r, negate, wild, nullpat, xflags;

  negate = *pat == L'!';
  wild = *pat == L'*' || *pat == L'?';
  pp = pat + 2;
  se = pp + wcslen (pp);
  pe = glob_patscan_wc (pp, se, 0);

   
  if (pe == 0)
    return 0;

  xflags = flags | ( negate ? GX_NEGATE : 0);

   
  if (pe == se && *pe == L'\0' && pe[-1] == L')' && (t = wcschr (pp, L'|')) == 0)
    {
      pe[-1] = L'\0';
      r = wskipname (pp, dname, xflags);  
      pe[-1] = L')';
      return r;
    }

   
  nullpat = pe >= (pat + 2) && pe[-2] == L'(' && pe[-1] == L')';

   
  while (t = glob_patscan_wc (pp, pe, '|'))
    {
      n = t[-1];	 
      if (wextglob_pattern_p (pp) && n == L')')		 
	t[-1] = n;	 
      else
	t[-1] = L'\0';
      r = wskipname (pp, dname, xflags);
      t[-1] = n;
      if (r == 0)
	return 0;
      pp = t;
      if (pp == pe)
	break;
    }

   
  if (pp == se)
    return r;

   
  if (wild && *pe != L'\0')
    return (wskipname (pe, dname, flags));

  return 1;
#else
  return (wskipname (pat, dname, flags));
#endif
}

 
static int
mbskipname (pat, dname, flags)
     char *pat, *dname;
     int flags;
{
  int ret, ext;
  wchar_t *pat_wc, *dn_wc;
  size_t pat_n, dn_n;

  if (mbsmbchar (dname) == 0 && mbsmbchar (pat) == 0)
    return (skipname (pat, dname, flags));

  ext = 0;
#if EXTENDED_GLOB
  ext = extglob_pattern_p (pat);
#endif

  pat_wc = dn_wc = (wchar_t *)NULL;

  pat_n = xdupmbstowcs (&pat_wc, NULL, pat);
  if (pat_n != (size_t)-1)
    dn_n = xdupmbstowcs (&dn_wc, NULL, dname);

  ret = 0;
  if (pat_n != (size_t)-1 && dn_n !=(size_t)-1)
    ret = ext ? wextglob_skipname (pat_wc, dn_wc, flags) : wskipname (pat_wc, dn_wc, flags);
  else
    ret = skipname (pat, dname, flags);

  FREE (pat_wc);
  FREE (dn_wc);

  return ret;
}
#endif  

 
void
udequote_pathname (pathname)
     char *pathname;
{
  register int i, j;

  for (i = j = 0; pathname && pathname[i]; )
    {
      if (pathname[i] == '\\')
	i++;

      pathname[j++] = pathname[i++];

      if (pathname[i - 1] == 0)
	break;
    }
  if (pathname)
    pathname[j] = '\0';
}

#if HANDLE_MULTIBYTE
 
void
wcdequote_pathname (wpathname)
     wchar_t *wpathname;
{
  int i, j;

  for (i = j = 0; wpathname && wpathname[i]; )
    {
      if (wpathname[i] == L'\\')
	i++;

      wpathname[j++] = wpathname[i++];

      if (wpathname[i - 1] == L'\0')
	break;
    }
  if (wpathname)
    wpathname[j] = L'\0';
}

static void
wdequote_pathname (pathname)
     char *pathname;
{
  mbstate_t ps;
  size_t len, n;
  wchar_t *wpathname;
  int i, j;
  wchar_t *orig_wpathname;

  if (mbsmbchar (pathname) == 0)
    {
      udequote_pathname (pathname);
      return;
    }

  len = strlen (pathname);
   
  n = xdupmbstowcs (&wpathname, NULL, pathname);
  if (n == (size_t) -1)
    {
       
      udequote_pathname (pathname);
      return;
    }
  orig_wpathname = wpathname;

  wcdequote_pathname (wpathname);

   
  memset (&ps, '\0', sizeof(mbstate_t));
  n = wcsrtombs(pathname, (const wchar_t **)&wpathname, len, &ps);
  if (n == (size_t)-1 || (wpathname && *wpathname != 0))	 
    {
      wpathname = orig_wpathname;
      memset (&ps, '\0', sizeof(mbstate_t));
      n = xwcsrtombs (pathname, (const wchar_t **)&wpathname, len, &ps);
    }
  pathname[len] = '\0';

   
  free (orig_wpathname);
}

static void
dequote_pathname (pathname)
     char *pathname;
{
  if (MB_CUR_MAX > 1)
    wdequote_pathname (pathname);
  else
    udequote_pathname (pathname);
}
#endif  

 

#if defined (HAVE_LSTAT)
#  define GLOB_TESTNAME(name)  (lstat (name, &finfo))
#else  
#  if !defined (AFS)
#    define GLOB_TESTNAME(name)  (sh_eaccess (name, F_OK))
#  else  
#    define GLOB_TESTNAME(name)  (access (name, F_OK))
#  endif  
#endif  

 
static int
glob_testdir (dir, flags)
     char *dir;
     int flags;
{
  struct stat finfo;
  int r;

 
#if defined (HAVE_LSTAT)
  r = (flags & GX_ALLDIRS) ? lstat (dir, &finfo) : stat (dir, &finfo);
#else
  r = stat (dir, &finfo);
#endif
  if (r < 0)
    return (-1);

#if defined (S_ISLNK)
  if (S_ISLNK (finfo.st_mode))
    return (-2);
#endif

  if (S_ISDIR (finfo.st_mode) == 0)
    return (-1);

  return (0);
}

 
static struct globval *
finddirs (pat, sdir, flags, ep, np)
     char *pat;
     char *sdir;
     int flags;
     struct globval **ep;
     int *np;
{
  char **r, *n;
  int ndirs;
  struct globval *ret, *e, *g;

 
  e = ret = 0;
  r = glob_vector (pat, sdir, flags);
  if (r == 0 || r[0] == 0)
    {
      if (np)
	*np = 0;
      if (ep)
        *ep = 0;
      if (r && r != &glob_error_return)
	free (r);
      return (struct globval *)0;
    }
  for (ndirs = 0; r[ndirs] != 0; ndirs++)
    {
      g = (struct globval *) malloc (sizeof (struct globval));
      if (g == 0)
	{
	  while (ret)		 
	    {
	      g = ret->next;
	      free (ret);
	      ret = g;
	    }

	  free (r);
	  if (np)
	    *np = 0;
	  if (ep)
	    *ep = 0;
	  return (&finddirs_error_return);
	}
      if (e == 0)
	e = g;

      g->next = ret;
      ret = g;

      g->name = r[ndirs];
    }

  free (r);
  if (ep)
    *ep = e;
  if (np)
    *np = ndirs;

  return ret;
}
     	
 

char **
glob_vector (pat, dir, flags)
     char *pat;
     char *dir;
     int flags;
{
  DIR *d;
  register struct dirent *dp;
  struct globval *lastlink, *e, *dirlist;
  register struct globval *nextlink;
  register char *nextname, *npat, *subdir;
  unsigned int count;
  int lose, skip, ndirs, isdir, sdlen, add_current, patlen;
  register char **name_vector;
  register unsigned int i;
  int mflags;		 
  int pflags;		 
  int hasglob;		 
  int nalloca;
  struct globval *firstmalloc, *tmplink;
  char *convfn;

  lastlink = 0;
  count = lose = skip = add_current = 0;

  firstmalloc = 0;
  nalloca = 0;

  name_vector = NULL;

 
   
  if (pat == 0 || *pat == '\0')
    {
      if (glob_testdir (dir, 0) < 0)
	return ((char **) &glob_error_return);

      nextlink = (struct globval *)alloca (sizeof (struct globval));
      if (nextlink == NULL)
	return ((char **) NULL);

      nextlink->next = (struct globval *)0;
      nextname = (char *) malloc (1);
      if (nextname == 0)
	lose = 1;
      else
	{
	  lastlink = nextlink;
	  nextlink->name = nextname;
	  nextname[0] = '\0';
	  count = 1;
	}

      skip = 1;
    }

  patlen = (pat && *pat) ? strlen (pat) : 0;

   
  hasglob = 0;
  if (skip == 0 && ((hasglob = glob_pattern_p (pat)) == 0 || hasglob == 2))
    {
      int dirlen;
      struct stat finfo;

      if (glob_testdir (dir, 0) < 0)
	return ((char **) &glob_error_return);

      dirlen = strlen (dir);
      nextname = (char *)malloc (dirlen + patlen + 2);
      npat = (char *)malloc (patlen + 1);
      if (nextname == 0 || npat == 0)
	{
	  FREE (nextname);
	  FREE (npat);
	  lose = 1;
	}
      else
	{
	  strcpy (npat, pat);
	  dequote_pathname (npat);

	  strcpy (nextname, dir);
	  nextname[dirlen++] = '/';
	  strcpy (nextname + dirlen, npat);

	  if (GLOB_TESTNAME (nextname) >= 0)
	    {
	      free (nextname);
	      nextlink = (struct globval *)alloca (sizeof (struct globval));
	      if (nextlink)
		{
		  nextlink->next = (struct globval *)0;
		  lastlink = nextlink;
		  nextlink->name = npat;
		  count = 1;
		}
	      else
		{
		  free (npat);
		  lose = 1;
		}
	    }
	  else
	    {
	      free (nextname);
	      free (npat);
	    }
	}

      skip = 1;
    }

  if (skip == 0)
    {
       
#if defined (OPENDIR_NOT_ROBUST)
      if (glob_testdir (dir, 0) < 0)
	return ((char **) &glob_error_return);
#endif

      d = opendir (dir);
      if (d == NULL)
	return ((char **) &glob_error_return);

       
      mflags = (noglob_dot_filenames ? FNM_PERIOD : FNM_DOTDOT) | FNM_PATHNAME;

#ifdef FNM_CASEFOLD
      if (glob_ignore_case)
	mflags |= FNM_CASEFOLD;
#endif

      if (extended_glob)
	mflags |= FNM_EXTMATCH;

      add_current = ((flags & (GX_ALLDIRS|GX_ADDCURDIR)) == (GX_ALLDIRS|GX_ADDCURDIR));

       
      while (1)
	{
	   
	  if (interrupt_state || terminating_signal)
	    {
	      lose = 1;
	      break;
	    }
	  else if (signal_is_pending (SIGINT))	 
	    {
	      lose = 1;
	      break;
	    }

	  dp = readdir (d);
	  if (dp == NULL)
	    break;

	   
	  if (REAL_DIR_ENTRY (dp) == 0)
	    continue;

#if 0
	  if (dp->d_name == 0 || *dp->d_name == 0)
	    continue;
#endif

#if HANDLE_MULTIBYTE
	  if (MB_CUR_MAX > 1 && mbskipname (pat, dp->d_name, flags))
	    continue;
	  else
#endif
	  if (skipname (pat, dp->d_name, flags))
	    continue;

	   
	  if (flags & (GX_MATCHDIRS|GX_ALLDIRS))
	    {
	      pflags = (flags & GX_ALLDIRS) ? MP_RMDOT : 0;
	      if (flags & GX_NULLDIR)
		pflags |= MP_IGNDOT;
	      subdir = sh_makepath (dir, dp->d_name, pflags);
	      isdir = glob_testdir (subdir, flags);
	      if (isdir < 0 && (flags & GX_MATCHDIRS))
		{
		  free (subdir);
		  continue;
		}
	    }

	  if (flags & GX_ALLDIRS)
	    {
	      if (isdir == 0)
		{
		  dirlist = finddirs (pat, subdir, (flags & ~GX_ADDCURDIR), &e, &ndirs);
		  if (dirlist == &finddirs_error_return)
		    {
		      free (subdir);
		      lose = 1;
		      break;
		    }
		  if (ndirs)		 
		    {
		      if (firstmalloc == 0)
		        firstmalloc = e;
		      e->next = lastlink;
		      lastlink = dirlist;
		      count += ndirs;
		    }
		}

	       
	      nextlink = (struct globval *) malloc (sizeof (struct globval));
	      if (firstmalloc == 0)
		firstmalloc = nextlink;
	      sdlen = strlen (subdir);
	      nextname = (char *) malloc (sdlen + 1);
	      if (nextlink == 0 || nextname == 0)
		{
		  if (firstmalloc && firstmalloc == nextlink)
		    firstmalloc = 0;
		   
		  FREE (nextlink);
		  FREE (nextname);
		  free (subdir);
		  lose = 1;
		  break;
		}
	      nextlink->next = lastlink;
	      lastlink = nextlink;
	      nextlink->name = nextname;
	      bcopy (subdir, nextname, sdlen + 1);
	      free (subdir);
	      ++count;
	      continue;
	    }
	  else if (flags & GX_MATCHDIRS)
	    free (subdir);

	  convfn = fnx_fromfs (dp->d_name, D_NAMLEN (dp));
	  if (strmatch (pat, convfn, mflags) != FNM_NOMATCH)
	    {
	      if (nalloca < ALLOCA_MAX)
		{
		  nextlink = (struct globval *) alloca (sizeof (struct globval));
		  nalloca += sizeof (struct globval);
		}
	      else
		{
		  nextlink = (struct globval *) malloc (sizeof (struct globval));
		  if (firstmalloc == 0)
		    firstmalloc = nextlink;
		}

	      nextname = (char *) malloc (D_NAMLEN (dp) + 1);
	      if (nextlink == 0 || nextname == 0)
		{
		   
		  if (firstmalloc)
		    {
		      if (firstmalloc == nextlink)
			firstmalloc = 0;
		      FREE (nextlink);
		    }
		  FREE (nextname);
		  lose = 1;
		  break;
		}
	      nextlink->next = lastlink;
	      lastlink = nextlink;
	      nextlink->name = nextname;
	      bcopy (dp->d_name, nextname, D_NAMLEN (dp) + 1);
	      ++count;
	    }
	}

      (void) closedir (d);
    }

   
  if (add_current && lose == 0)
    {
      sdlen = strlen (dir);
      nextname = (char *)malloc (sdlen + 1);
      nextlink = (struct globval *) malloc (sizeof (struct globval));
      if (nextlink == 0 || nextname == 0)
	{
	  FREE (nextlink);
	  FREE (nextname);
	  lose = 1;
	}
      else
	{
	  nextlink->name = nextname;
	  nextlink->next = lastlink;
	  lastlink = nextlink;
	  if (flags & GX_NULLDIR)
	    nextname[0] = '\0';
	  else
	    bcopy (dir, nextname, sdlen + 1);
	  ++count;
	}
    }

  if (lose == 0)
    {
      name_vector = (char **) malloc ((count + 1) * sizeof (char *));
      lose |= name_vector == NULL;
    }

   
  if (lose)
    {
      tmplink = 0;

       
      while (lastlink)
	{
	   
	  if (firstmalloc)
	    {
	      if (lastlink == firstmalloc)
		firstmalloc = 0;
	      tmplink = lastlink;
	    }
	  else
	    tmplink = 0;
	  free (lastlink->name);
	  lastlink = lastlink->next;
	  FREE (tmplink);
	}

       

      return ((char **)NULL);
    }

   
  for (tmplink = lastlink, i = 0; i < count; ++i)
    {
      name_vector[i] = tmplink->name;
      tmplink = tmplink->next;
    }

  name_vector[count] = NULL;

   
  if (firstmalloc)
    {
      tmplink = 0;
      while (lastlink)
	{
	  tmplink = lastlink;
	  if (lastlink == firstmalloc)
	    lastlink = firstmalloc = 0;
	  else
	    lastlink = lastlink->next;
	  free (tmplink);
	}
    }

  return (name_vector);
}

 
static char **
glob_dir_to_array (dir, array, flags)
     char *dir, **array;
     int flags;
{
  register unsigned int i, l;
  int add_slash;
  char **result, *new;
  struct stat sb;

  l = strlen (dir);
  if (l == 0)
    {
      if (flags & GX_MARKDIRS)
	for (i = 0; array[i]; i++)
	  {
	    if ((stat (array[i], &sb) == 0) && S_ISDIR (sb.st_mode))
	      {
		l = strlen (array[i]);
		new = (char *)realloc (array[i], l + 2);
		if (new == 0)
		  return NULL;
		new[l] = '/';
		new[l+1] = '\0';
		array[i] = new;
	      }
	  }
      return (array);
    }

  add_slash = dir[l - 1] != '/';

  i = 0;
  while (array[i] != NULL)
    ++i;

  result = (char **) malloc ((i + 1) * sizeof (char *));
  if (result == NULL)
    return (NULL);

  for (i = 0; array[i] != NULL; i++)
    {
       
      result[i] = (char *) malloc (l + strlen (array[i]) + 3);

      if (result[i] == NULL)
	{
	  int ind;
	  for (ind = 0; ind < i; ind++)
	    free (result[ind]);
	  free (result);
	  return (NULL);
	}

      strcpy (result[i], dir);
      if (add_slash)
	result[i][l] = '/';
      if (array[i][0])
	{
	  strcpy (result[i] + l + add_slash, array[i]);
	  if (flags & GX_MARKDIRS)
	    {
	      if ((stat (result[i], &sb) == 0) && S_ISDIR (sb.st_mode))
		{
		  size_t rlen;
		  rlen = strlen (result[i]);
		  result[i][rlen] = '/';
		  result[i][rlen+1] = '\0';
		}
	    }
	}
      else
        result[i][l+add_slash] = '\0';
    }
  result[i] = NULL;

   
  for (i = 0; array[i] != NULL; i++)
    free (array[i]);
  free ((char *) array);

  return (result);
}

 
char **
glob_filename (pathname, flags)
     char *pathname;
     int flags;
{
  char **result, **new_result;
  unsigned int result_size;
  char *directory_name, *filename, *dname, *fn;
  unsigned int directory_len;
  int free_dirname;			 
  int dflags, hasglob;

  result = (char **) malloc (sizeof (char *));
  result_size = 1;
  if (result == NULL)
    return (NULL);

  result[0] = NULL;

  directory_name = NULL;

   
  filename = strrchr (pathname, '/');
#if defined (EXTENDED_GLOB)
  if (filename && extended_glob)
    {
      fn = glob_dirscan (pathname, '/');
#if DEBUG_MATCHING
      if (fn != filename)
	fprintf (stderr, "glob_filename: glob_dirscan: fn (%s) != filename (%s)\n", fn ? fn : "(null)", filename);
#endif
      filename = fn;
    }
#endif

  if (filename == NULL)
    {
      filename = pathname;
      directory_name = "";
      directory_len = 0;
      free_dirname = 0;
    }
  else
    {
      directory_len = (filename - pathname) + 1;
      directory_name = (char *) malloc (directory_len + 1);

      if (directory_name == 0)		 
	{
	  free (result);
	  return (NULL);
	}

      bcopy (pathname, directory_name, directory_len);
      directory_name[directory_len] = '\0';
      ++filename;
      free_dirname = 1;
    }

  hasglob = 0;
   
  if (directory_len > 0 && (hasglob = glob_pattern_p (directory_name)) == 1)
    {
      char **directories, *d, *p;
      register unsigned int i;
      int all_starstar, last_starstar;

      all_starstar = last_starstar = 0;
      d = directory_name;
      dflags = flags & ~GX_MARKDIRS;
       
      if ((flags & GX_GLOBSTAR) && d[0] == '*' && d[1] == '*' && (d[2] == '/' || d[2] == '\0'))
	{
	  p = d;
	  while (d[0] == '*' && d[1] == '*' && (d[2] == '/' || d[2] == '\0'))
	    {
	      p = d;
	      if (d[2])
		{
		  d += 3;
		  while (*d == '/')
		    d++;
		  if (*d == 0)
		    break;
		}
	    }
	  if (*d == 0)
	    all_starstar = 1;
	  d = p;
	  dflags |= GX_ALLDIRS|GX_ADDCURDIR;
	  directory_len = strlen (d);
	}

       
      if ((flags & GX_GLOBSTAR) && all_starstar == 0)
	{
	  int dl, prev;
	  prev = dl = directory_len;
	  while (dl >= 4 && d[dl - 1] == '/' &&
			   d[dl - 2] == '*' &&
			   d[dl - 3] == '*' &&
			   d[dl - 4] == '/')
	    prev = dl, dl -= 3;
	  if (dl != directory_len)
	    last_starstar = 1;
	  directory_len = prev;
	}

       
      if (last_starstar && directory_len > 4 &&
	    filename[0] == '*' && filename[1] == '*' && filename[2] == 0)
	{
	  directory_len -= 3;
	}

      if (d[directory_len - 1] == '/')
	d[directory_len - 1] = '\0';

      directories = glob_filename (d, dflags|GX_RECURSE);

      if (free_dirname)
	{
	  free (directory_name);
	  directory_name = NULL;
	}

      if (directories == NULL)
	goto memory_error;
      else if (directories == (char **)&glob_error_return)
	{
	  free ((char *) result);
	  return ((char **) &glob_error_return);
	}
      else if (*directories == NULL)
	{
	  free ((char *) directories);
	  free ((char *) result);
	  return ((char **) &glob_error_return);
	}

       
      if (all_starstar && filename[0] == '*' && filename[1] == '*' && filename[2] == 0)
	{
	  free ((char *) directories);
	  free (directory_name);
	  directory_name = NULL;
	  directory_len = 0;
	  goto only_filename;
	}

       
      for (i = 0; directories[i] != NULL; ++i)
	{
	  char **temp_results;
	  int shouldbreak;

	  shouldbreak = 0;
	   
	   
	  dname = directories[i];
	  dflags = flags & ~(GX_MARKDIRS|GX_ALLDIRS|GX_ADDCURDIR);
	   
	  if ((flags & GX_GLOBSTAR) && filename[0] == '*' && filename[1] == '*' && filename[2] == '\0')
	    dflags |= GX_ALLDIRS|GX_ADDCURDIR;
	  if (dname[0] == '\0' && filename[0])
	    {
	      dflags |= GX_NULLDIR;
	      dname = ".";	 
	    }

	   
	  if (all_starstar && (dflags & GX_NULLDIR) == 0)
	    {
	      int dlen;

	       
	      if (glob_testdir (dname, flags|GX_ALLDIRS) == -2 && glob_testdir (dname, 0) == 0)
		{
		  if (filename[0] != 0)
		    temp_results = (char **)&glob_error_return;		 
		  else
		    {
		       
		      temp_results = (char **)malloc (2 * sizeof (char *));
		      if (temp_results == NULL)
			goto memory_error;
		      temp_results[0] = (char *)malloc (1);
		      if (temp_results[0] == 0)
			{
			  free (temp_results);
			  goto memory_error;
			}
		      **temp_results = '\0';
		      temp_results[1] = NULL;
		      dflags |= GX_SYMLINK;	 
		    }
		}
	      else
		temp_results = glob_vector (filename, dname, dflags);
	    }
	  else
	    temp_results = glob_vector (filename, dname, dflags);

	   
	  if (temp_results == NULL)
	    goto memory_error;
	  else if (temp_results == (char **)&glob_error_return)
	     
	    ;
	  else
	    {
	      char **array;
	      register unsigned int l;

	       
	      if ((dflags & GX_ALLDIRS) && filename[0] == '*' && filename[1] == '*' && (filename[2] == '\0' || filename[2] == '/'))
		{
		   
		   
		   
#define NULL_PLACEHOLDER(x)	((x) && *(x) && **(x) == 0)
		  if ((dflags & GX_NULLDIR) && (flags & GX_NULLDIR) == 0 &&
			NULL_PLACEHOLDER (temp_results))
#undef NULL_PLACEHOLDER
		    {
		      register int i, n;
		      for (n = 0; temp_results[n] && *temp_results[n] == 0; n++)
			;
		      i = n;
		      do
			temp_results[i - n] = temp_results[i];
		      while (temp_results[i++] != 0);
		      array = temp_results;
		      shouldbreak = 1;
		    }
	          else
		    array = temp_results;
		}
	      else if (dflags & GX_SYMLINK)
		array = glob_dir_to_array (directories[i], temp_results, flags);
	      else
		array = glob_dir_to_array (directories[i], temp_results, flags);
	      l = 0;
	      while (array[l] != NULL)
		++l;

	      new_result = (char **)realloc (result, (result_size + l) * sizeof (char *));

	      if (new_result == NULL)
		{
		  for (l = 0; array[l]; ++l)
		    free (array[l]);
		  free ((char *)array);
		  goto memory_error;
		}
	      result = new_result;

	      for (l = 0; array[l] != NULL; ++l)
		result[result_size++ - 1] = array[l];

	      result[result_size - 1] = NULL;

	       
	      if (array != temp_results)
		free ((char *) array);
	      else if ((dflags & GX_ALLDIRS) && filename[0] == '*' && filename[1] == '*' && filename[2] == '\0')
		free (temp_results);	 

	      if (shouldbreak)
		break;
	    }
	}
       
      for (i = 0; directories[i]; i++)
	free (directories[i]);

      free ((char *) directories);

      return (result);
    }

only_filename:
   
  if (*filename == '\0')
    {
      result = (char **) realloc ((char *) result, 2 * sizeof (char *));
      if (result == NULL)
	{
	  if (free_dirname)
	    free (directory_name);
	  return (NULL);
	}
       
      if (directory_len > 0 && hasglob == 2 && (flags & GX_RECURSE) != 0)
	{
	  dequote_pathname (directory_name);
	  directory_len = strlen (directory_name);
	}

       

      if (directory_len > 0 && hasglob == 2 && (flags & GX_RECURSE) == 0)
	{
	  dequote_pathname (directory_name);
	  if (glob_testdir (directory_name, 0) < 0)
	    {
	      if (free_dirname)
		free (directory_name);
	      free ((char *) result);
	      return ((char **)&glob_error_return);
	    }
	}

       
      result[0] = (char *) malloc (directory_len + 1);
      if (result[0] == NULL)
	goto memory_error;
      bcopy (directory_name, result[0], directory_len + 1);
      if (free_dirname)
	free (directory_name);
      result[1] = NULL;
      return (result);
    }
  else
    {
      char **temp_results;

       
      if (directory_len > 0)
	dequote_pathname (directory_name);

       
      free (result);

       
       
      dflags = flags & ~GX_MARKDIRS;
      if (directory_len == 0)
	dflags |= GX_NULLDIR;
      if ((flags & GX_GLOBSTAR) && filename[0] == '*' && filename[1] == '*' && filename[2] == '\0')
	{
	  dflags |= GX_ALLDIRS|GX_ADDCURDIR;
#if 0
	   
#endif
	  if (directory_len == 0 && (flags & GX_ALLDIRS) == 0)
	    dflags &= ~GX_ADDCURDIR;
	}
      temp_results = glob_vector (filename,
				  (directory_len == 0 ? "." : directory_name),
				  dflags);

      if (temp_results == NULL || temp_results == (char **)&glob_error_return)
	{
	  if (free_dirname)
	    free (directory_name);
	  QUIT;			 
	  run_pending_traps ();
	  return (temp_results);
	}

      result = glob_dir_to_array ((dflags & GX_ALLDIRS) ? "" : directory_name, temp_results, flags);

      if (free_dirname)
	free (directory_name);
      return (result);
    }

   
 memory_error:
  if (result != NULL)
    {
      register unsigned int i;
      for (i = 0; result[i] != NULL; ++i)
	free (result[i]);
      free ((char *) result);
    }

  if (free_dirname && directory_name)
    free (directory_name);

  QUIT;
  run_pending_traps ();

  return (NULL);
}

#if defined (TEST)

main (argc, argv)
     int argc;
     char **argv;
{
  unsigned int i;

  for (i = 1; i < argc; ++i)
    {
      char **value = glob_filename (argv[i], 0);
      if (value == NULL)
	puts ("Out of memory.");
      else if (value == &glob_error_return)
	perror (argv[i]);
      else
	for (i = 0; value[i] != NULL; i++)
	  puts (value[i]);
    }

  exit (0);
}
#endif	 
