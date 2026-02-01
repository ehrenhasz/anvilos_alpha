 

 

#define READLINE_LIBRARY

#if defined (HAVE_CONFIG_H)
#  include <config.h>
#endif

#include <sys/types.h>

#include <stdio.h>

#if defined (HAVE_UNISTD_H)
#  include <unistd.h>
#endif  

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if defined (HAVE_LOCALE_H)
#  include <locale.h>
#endif

#if defined (HAVE_LANGINFO_CODESET)
#  include <langinfo.h>
#endif

#include <ctype.h>

#include "rldefs.h"
#include "readline.h"
#include "rlshell.h"
#include "rlprivate.h"
#include "xmalloc.h"

static int utf8locale (char *);

#define RL_DEFAULT_LOCALE "C"
static char *_rl_current_locale = 0;

#if !defined (HAVE_SETLOCALE)    
 
static char *legal_lang_values[] =
{
 "iso88591",
 "iso88592",
 "iso88593",
 "iso88594",
 "iso88595",
 "iso88596",
 "iso88597",
 "iso88598",
 "iso88599",
 "iso885910",
 "koi8r",
 "utf8",
  0
};

static char *normalize_codeset (char *);
#endif  

static char *find_codeset (char *, size_t *);

static char *_rl_get_locale_var (const char *);

static char *
_rl_get_locale_var (const char *v)
{
  char *lspec;

  lspec = sh_get_env_value ("LC_ALL");
  if (lspec == 0 || *lspec == 0)
    lspec = sh_get_env_value (v);
  if (lspec == 0 || *lspec == 0)
    lspec = sh_get_env_value ("LANG");

  return lspec;
}

static int
utf8locale (char *lspec)
{
  char *cp;
  size_t len;

#if HAVE_LANGINFO_CODESET
  cp = nl_langinfo (CODESET);
  return (STREQ (cp, "UTF-8") || STREQ (cp, "utf8"));
#else
  cp = find_codeset (lspec, &len);

  if (cp == 0 || len < 4 || len > 5)
    return 0;
  return ((len == 5) ? strncmp (cp, "UTF-8", len) == 0 : strncmp (cp, "utf8", 4) == 0);
#endif
}

 
char *
_rl_init_locale (void)
{
  char *ret, *lspec;

   
  lspec = _rl_get_locale_var ("LC_CTYPE");
   
#if defined (HAVE_SETLOCALE)
  if (lspec == 0 || *lspec == 0)
    lspec = setlocale (LC_CTYPE, (char *)NULL);
  if (lspec == 0)
    lspec = "";
  ret = setlocale (LC_CTYPE, lspec);	 
#else
  ret = (lspec == 0 || *lspec == 0) ? RL_DEFAULT_LOCALE : lspec;
#endif

  _rl_utf8locale = (ret && *ret) ? utf8locale (ret) : 0;

  _rl_current_locale = savestring (ret);
  return ret;
}

 
static int
_rl_set_localevars (char *localestr, int force)
{
#if defined (HAVE_SETLOCALE)
  if (localestr && *localestr && (localestr[0] != 'C' || localestr[1]) && (STREQ (localestr, "POSIX") == 0))
    {
      _rl_meta_flag = 1;
      _rl_convert_meta_chars_to_ascii = 0;
      _rl_output_meta_chars = 1;
      return (1);
    }
  else if (force)
    {
       
      _rl_meta_flag = 0;
      _rl_convert_meta_chars_to_ascii = 1;
      _rl_output_meta_chars = 0;
      return (0);
    }
  else
    return (0);

#else  
  char *t;
  int i;

   
  if (localestr == 0 || (t = normalize_codeset (localestr)) == 0)
    return (0);
  for (i = 0; t && legal_lang_values[i]; i++)
    if (STREQ (t, legal_lang_values[i]))
      {
	_rl_meta_flag = 1;
	_rl_convert_meta_chars_to_ascii = 0;
	_rl_output_meta_chars = 1;
	break;
      }

  if (force && legal_lang_values[i] == 0)	 
    {
       
      _rl_meta_flag = 0;
      _rl_convert_meta_chars_to_ascii = 1;
      _rl_output_meta_chars = 0;
    }

  _rl_utf8locale = *t ? STREQ (t, "utf8") : 0;

  xfree (t);
  return (legal_lang_values[i] ? 1 : 0);
#endif  
}

 
int
_rl_init_eightbit (void)
{
  char *t, *ol;

  ol = _rl_current_locale;
  t = _rl_init_locale ();	 
  xfree (ol);

  return (_rl_set_localevars (t, 0));
}

#if !defined (HAVE_SETLOCALE)
static char *
normalize_codeset (char *codeset)
{
  size_t namelen, i;
  int len, all_digits;
  char *wp, *retval;

  codeset = find_codeset (codeset, &namelen);

  if (codeset == 0)
    return (codeset);

  all_digits = 1;
  for (len = 0, i = 0; i < namelen; i++)
    {
      if (ISALNUM ((unsigned char)codeset[i]))
	{
	  len++;
	  all_digits &= _rl_digit_p (codeset[i]);
	}
    }

  retval = (char *)malloc ((all_digits ? 3 : 0) + len + 1);
  if (retval == 0)
    return ((char *)0);

  wp = retval;
   
  if (all_digits)
    {
      *wp++ = 'i';
      *wp++ = 's';
      *wp++ = 'o';
    }

  for (i = 0; i < namelen; i++)
    if (ISALPHA ((unsigned char)codeset[i]))
      *wp++ = _rl_to_lower (codeset[i]);
    else if (_rl_digit_p (codeset[i]))
      *wp++ = codeset[i];
  *wp = '\0';

  return retval;
}
#endif  

 
static char *
find_codeset (char *name, size_t *lenp)
{
  char *cp, *language, *result;

  cp = language = name;
  result = (char *)0;

  while (*cp && *cp != '_' && *cp != '@' && *cp != '+' && *cp != ',')
    cp++;

   
  if (language == cp) 
    {
      *lenp = strlen (language);
      result = language;
    }
  else
    {
       
      if (*cp == '_')
	do
	  ++cp;
	while (*cp && *cp != '.' && *cp != '@' && *cp != '+' && *cp != ',' && *cp != '_');

       
      result = cp;
      if (*cp == '.')
	do
	  ++cp;
	while (*cp && *cp != '@');

      if (cp - result > 2)
	{
	  result++;
	  *lenp = cp - result;
	}
      else
	{
	  *lenp = strlen (language);
	  result = language;
	}
    }

  return result;
}

void
_rl_reset_locale (void)
{
  char *ol, *nl;

   
  ol = _rl_current_locale;
  nl = _rl_init_locale ();		 

  if ((ol == 0 && nl) || (ol && nl && (STREQ (ol, nl) == 0)))
    (void)_rl_set_localevars (nl, 1);

  xfree (ol);
}
